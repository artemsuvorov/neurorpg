#pragma once

namespace Neuro::Net {

	class Server final
	{
	public:
		struct Config
		{
			uint16_t Port = 0;
		};

	public:
		explicit Server(Config&& config);
		~Server();

		void Start();
		void Stop();

		bool IsRunning() const;

	private:
		struct Session;
		struct Acceptor;
		struct Impl;

		std::unique_ptr<Impl> m_Impl;
	};

}  // namespace Neuro::Net
