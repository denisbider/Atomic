#include "AutIncludes.h"


#pragma warning (disable: 4702) // Spurious unreachable code warning

void SchannelClientTest(Slice<Seq> args)
{
	if (args.Len() < 3)
		throw "Missing <host:port> parameter";

	struct ServerType { enum E { Http, Smtp }; };

	ServerType::E serverType  { ServerType::Http };
	uint          defaultPort { 443 };
	bool          serverAuth  { true };
	Seq           hostPort    { args[2] };
	Seq           host        { Seq(hostPort).ReadToByte(':') };

	for (sizet i=3; i!=args.Len(); ++i)
	{
		Seq arg { args[i] };
		     if (arg.EqualInsensitive("-smtp"))         { serverType = ServerType::Smtp; defaultPort = 25; }
		else if (arg.EqualInsensitive("-noserverauth")) { serverAuth = false; }
		else throw "Unrecognized switch or parameter";
	}

	try
	{
		SockInit sockInit;

		Rp<StopCtl>        stopCtl { new StopCtl };
		ThreadPtr<WaitEsc> waitEsc { Thread::Create };
		waitEsc->Start(stopCtl);

		SocketConnector conn;
		conn.SetStopCtl(stopCtl);

		Socket sk;
		sk.SetSocket(conn.ConnectWithLookup(hostPort, defaultPort, IpVerPreference::None));
		Console::Out(Str("Connected to ").Obj(sk.RemoteAddr(), SockAddr::AddrPort).Add("\r\n"));

		SocketReader reader { sk.GetSocket() };
		SocketWriter writer { sk.GetSocket() };
		Schannel sch { &reader, &writer };

		sch.SetStopCtl(stopCtl);

		if (serverType == ServerType::Http)
		{
			sch.InitCred(ProtoSide::Client);

			if (serverAuth)
				sch.ValidateServerName(host);

			sch.StartTls();

			Str req;
			req.Set("GET / HTTP/1.1\r\n"
					"Host: ").Add(hostPort).Add("\r\n"
					"Connection: close\r\n"
					"\r\n");

			sch.Write(req);

			while (true)
			{
				sch.Read( [] (Seq& data) -> Reader::Instr::E
					{
						Console::Out(Str("\r\n").HexDump(data.ReadBytes(SIZE_MAX), 0, 24));
						return Reader::Instr::Done;
					} );
			}
		}
		else if (serverType == ServerType::Smtp)
		{
			uint replyCode {};
			bool replyDone {};

			auto readReply = [&] (Seq& data) -> Reader::Instr::E
				{
					Seq line { data.ReadToString("\r\n") };
					if (data.n)
					{
						Console::Out(Str("Recv: ").Add(line).Add("\r\n"));
						replyCode = line.ReadNrUInt32Dec();
						replyDone = !line.StartsWithExact("-");
						data.DropBytes(2);
						return Reader::Instr::Done;
					}
					return Reader::Instr::NeedMore;
				};

			auto sendLineWaitReply = [&] (Seq line)
				{
					Console::Out(Str("Send: ").Add(line).Add("\r\n"));

					Str lineStr = Str::Join(line, "\r\n");
					sch.Write(lineStr);

					replyDone = false;
					while (!replyDone)
						sch.Read(readReply);
				};

			while (!replyDone)
				sch.Read(readReply);

			sendLineWaitReply("ehlo test");
			sendLineWaitReply("starttls");

			sch.InitCred(ProtoSide::Client);

			if (serverAuth)
				sch.ValidateServerName(host);

			sch.StartTls();

			sendLineWaitReply("ehlo test");
		}
		else
			EnsureAbort(!"Unrecognized server type");
	}
	catch (CommunicationErr const& e)
	{
		Console::Out(Str("Terminated by CommunicationErr:\r\n")
					.Add(e.what()).Add("\r\n"));
	}
	catch (std::exception const& e)
	{
		Console::Out(Str("Terminated by std::exception:\r\n")
					.Add(e.what()).Add("\r\n"));
	}
}
