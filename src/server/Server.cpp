#include "Precompiled.h"
#include "Server.h"
#include "WorkerPool.h"

#include <asio.hpp>
#include <atomic>
#include <thread>


using tcp = asio::ip::tcp;


static void PrintError(const std::error_code& error)
{
	printf("Error %u: %s\n", error.value(), error.message().c_str());
}


namespace Neuro::Net {

	class IWorkerDispatcher
	{
	public:
		virtual ~IWorkerDispatcher() = default;
		virtual void Enqueue(TTaskFunctor&& functor) = 0;
	};

}  // namespace Neuro::Net


namespace Neuro::Net {

	struct Server::Session final : public std::enable_shared_from_this<Session>
	{
	public:
		explicit Session(tcp::socket&& socket, IWorkerDispatcher& dispatcher)
		    : m_Socket(std::move(socket)), m_Dispatcher(dispatcher)
		{
		}

		void Run();

		asio::awaitable<void> Execute();

		asio::awaitable<std::string> Read(asio::error_code& error);
		asio::awaitable<void> Write(std::string message, asio::error_code& error);
		asio::awaitable<std::string> Dispatch(std::string input, asio::error_code& error);

	private:
		tcp::socket m_Socket;
		asio::streambuf m_Buffer;
		IWorkerDispatcher& m_Dispatcher;
	};


	void Server::Session::Run()
	{
		const auto Coroutine = [self = shared_from_this()]() -> asio::awaitable<void> {
			co_await self->Execute();
		};

		asio::co_spawn(m_Socket.get_executor(), Coroutine, asio::detached);
	}


	asio::awaitable<void> Server::Session::Execute()
	{
		// Keep session alive while coroutine runs.
		auto self = shared_from_this();
		asio::error_code error;

		while (true)
		{
			std::string message = co_await Read(error);
			if (error)
				break;
			printf("Received: %s\n", message.c_str());

			std::string response = co_await Dispatch(std::move(message), error);
			if (error)
				break;

			printf("Response: %s\n", response.c_str());
			co_await Write(std::move(response), error);
		}

		PrintError(error);
	}


	asio::awaitable<std::string> Server::Session::Read(asio::error_code& error)
	{
		constexpr char kDelimiter = '\n';
		co_await asio::async_read_until(m_Socket, m_Buffer, kDelimiter, asio::redirect_error(asio::use_awaitable, error));
		if (error)
			co_return "";

		std::istream stream(&m_Buffer);
		std::string line;
		std::getline(stream, line);
		co_return line;
	}


	asio::awaitable<void> Server::Session::Write(std::string message, asio::error_code& error)
	{
		co_await asio::async_write(m_Socket, asio::buffer(message), asio::redirect_error(asio::use_awaitable, error));
	}


	asio::awaitable<std::string> Server::Session::Dispatch(std::string input, asio::error_code& error)
	{
		auto self = shared_from_this();
		asio::any_io_executor executor = co_await asio::this_coro::executor;
		co_return co_await asio::async_initiate<decltype(asio::use_awaitable), void(std::error_code, std::string)>(
		    [self, input = std::move(input), executor](auto handler) mutable {
			    self->m_Dispatcher.Enqueue([input = std::move(input), executor, handler = std::move(handler)]() mutable {
				    std::this_thread::sleep_for(std::chrono::milliseconds(2'500));  // Simulate work.
				    std::string result = "Echo: " + input;
				    std::error_code error;
				    asio::post(executor, [handler = std::move(handler), error, result = std::move(result)]() mutable {
					    handler(error, std::move(result));
				    });
			    });
		    },
		    asio::use_awaitable);
	}

}  // namespace Neuro::Net


namespace Neuro::Net {

	struct Server::Acceptor final
	{
	public:
		Acceptor(asio::io_context& context, IWorkerDispatcher& dipatcher)
		    : m_Context(context), m_Acceptor(context), m_Dispatcher(dipatcher)
		{
		}

		bool Open(uint16_t port, asio::error_code& error);
		void Close();
		void Accept();

	private:
		asio::io_context& m_Context;
		tcp::acceptor m_Acceptor;
		IWorkerDispatcher& m_Dispatcher;
	};


	bool Server::Acceptor::Open(uint16_t port, asio::error_code& error)
	{
		m_Acceptor.open(asio::ip::tcp::v4(), error);
		if (error)
			return false;

		m_Acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), error);
		if (error)
			return false;

		m_Acceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port), error);
		if (error)
			return false;

		m_Acceptor.listen(asio::socket_base::max_listen_connections, error);
		return !error;
	}


	void Server::Acceptor::Close()
	{
		asio::error_code error;
		m_Acceptor.close(error);
		if (error)
			PrintError(error);
	}


	void Server::Acceptor::Accept()
	{
		m_Acceptor.async_accept([this](asio::error_code error, tcp::socket socket) mutable {
			if (error)
			{
				PrintError(error);
				return;
			}

			std::make_shared<Session>(std::move(socket), m_Dispatcher)->Run();
			Accept();
		});
	}

}  // namespace Neuro::Net


namespace Neuro::Net {

	struct Server::Impl final : public IWorkerDispatcher
	{
	public:
		Impl(Server::Config&& config) : m_Config(std::move(config)), m_Acceptor(m_Context, *this) {}
		~Impl();

		void Start();
		void Stop();

		bool IsRunning() const;

	public:
		virtual void Enqueue(TTaskFunctor&& functor) override;

	private:
		asio::io_context m_Context;
		Server::Config m_Config;
		Server::Acceptor m_Acceptor;
		std::unique_ptr<WorkerPool> m_Workers;
		std::atomic<bool> m_IsRunning = false;
		std::thread m_Thread;
	};


	Server::Impl::~Impl()
	{
		Stop();
	}


	void Server::Impl::Start()
	{
		if (m_IsRunning.load(std::memory_order::acquire))
			return;

		m_Context.restart();

		const uint16_t port = m_Config.Port;
		asio::error_code error;
		if (!m_Acceptor.Open(port, error))
		{
			PrintError(error);
			return;
		}

		printf("Server listening on port %u ...\n", port);
		m_IsRunning.store(true, std::memory_order::release);
		m_Workers = std::make_unique<WorkerPool>(4);

		m_Acceptor.Accept();

		m_Thread = std::thread([this]() {
			m_Context.run();
		});
	}


	void Server::Impl::Stop()
	{
		if (!m_IsRunning.load(std::memory_order::acquire))
			return;
		
		m_Acceptor.Close();
		m_Context.stop();
		if (m_Thread.joinable())
			m_Thread.join();
		
		m_Workers.reset();

		m_IsRunning.store(false, std::memory_order::release);
	}


	inline bool Server::Impl::IsRunning() const
	{
		return m_IsRunning.load(std::memory_order::acquire);
	}


	inline void Server::Impl::Enqueue(TTaskFunctor&& functor)
	{
		m_Workers->Enqueue(std::move(functor));
	}

}  // namespace Neuro::Net


namespace Neuro::Net {

	Server::Server(Config&& config) : m_Impl(std::make_unique<Impl>(std::move(config))) {}


	Server::~Server() = default;


	void Server::Start()
	{
		m_Impl->Start();
	}


	void Server::Stop()
	{
		m_Impl->Stop();
	}


	bool Server::IsRunning() const
	{
		return m_Impl->IsRunning();
	}

}  // namespace Neuro::Net
