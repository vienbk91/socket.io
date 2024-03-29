/****************************************************************************
Copyright (c) 2015 Chris Hannon http://www.channon.us
Copyright (c) 2013-2015 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*based on the SocketIO library created by LearnBoost at http://socket.io
*using spec version 1 found at https://github.com/LearnBoost/socket.io-spec

****************************************************************************/

#include "SocketIO.h"
#include "base/CCDirector.h"
#include "base/CCScheduler.h"
#include "WebSocket.h"
#include "HttpClient.h"
#include <algorithm>
#include <sstream>

#include "json/rapidjson.h"
#include "json/document.h"
#include "json/stringbuffer.h"
#include "json/writer.h"

NS_CC_BEGIN

namespace network {

	//class declarations

	class SocketIOPacketV10x;

	class SocketIOPacket
	{
	public:
		typedef enum
		{
			V09x,
			V10x
		}SocketIOVersion;

		SocketIOPacket();
		virtual ~SocketIOPacket();
		void initWithType(std::string packetType);
		void initWithTypeIndex(int index);

		std::string toString();
		virtual int typeAsNumber();
		std::string typeForIndex(int index);

		void setEndpoint(std::string endpoint){ _endpoint = endpoint; };
		std::string getEndpoint(){ return _endpoint; };
		void setEvent(std::string event){ _name = event; };
		std::string getEvent(){ return _name; };

		void addData(std::string data);
		std::vector<std::string> getData(){ return _args; };
		virtual std::string stringify();

		static SocketIOPacket * createPacketWithType(std::string type, SocketIOPacket::SocketIOVersion version);
		static SocketIOPacket * createPacketWithTypeIndex(int type, SocketIOPacket::SocketIOVersion version);
	protected:
		std::string _pId;//id message
		std::string _ack;//
		std::string _name;//event name
		std::vector<std::string> _args;//we will be using a vector of strings to store multiple data
		std::string _endpoint;//
		std::string _endpointseperator;//socket.io 1.x requires a ',' between endpoint and payload
		std::string _type;//message type
		std::string _separator;//for stringify the object
		std::vector<std::string> _types;//types of messages
	};

	class SocketIOPacketV10x : public SocketIOPacket
	{
	public:
		SocketIOPacketV10x();
		virtual ~SocketIOPacketV10x();
		int typeAsNumber();
		std::string stringify();
	private:
		std::vector<std::string> _typesMessage;
	};

	SocketIOPacket::SocketIOPacket()
	{
		_type = "";//message type
		_separator = ":";//for stringify the object
		_endpointseperator = "";//socket.io 1.x requires a ',' between endpoint and payload
		_pId = "";//id message
		_ack = "";//
		_name = "";//event name
		_endpoint = "";//
		_types.push_back("disconnect");
		_types.push_back("connect");
		_types.push_back("heartbeat");
		_types.push_back("message");
		_types.push_back("json");
		_types.push_back("event");
		_types.push_back("ack");
		_types.push_back("error");
		_types.push_back("noop");
	}

	SocketIOPacket::~SocketIOPacket()
	{
		_types.clear();
		_type = "";
		_pId = "";
		_name = "";
		_ack = "";
		_endpoint = "";
	}

	void SocketIOPacket::initWithType(std::string packetType)
	{
		_type = packetType;
	}
	void SocketIOPacket::initWithTypeIndex(int index)
	{
		_type = _types.at(index);
	}

	std::string SocketIOPacket::toString()
	{
		std::stringstream encoded;
		encoded << this->typeAsNumber();
		encoded << this->_separator;

		std::string pIdL = _pId;
		if (_ack == "data")
		{
			pIdL += "+";
		}

		// Do not write pid for acknowledgements
		if (_type != "ack")
		{
			encoded << pIdL;
		}
		encoded << this->_separator;

		// Add the endpoint for the namespace to be used if not the default namespace "" or "/", and as long as it is not an ACK, heartbeat, or disconnect packet
		if (_endpoint != "/" && _endpoint != "" && _type != "ack" && _type != "heartbeat" && _type != "disconnect") {
			encoded << _endpoint << _endpointseperator;
		}
		encoded << this->_separator;


		if (_args.size() != 0)
		{
			std::string ackpId = "";
			// This is an acknowledgement packet, so, prepend the ack pid to the data
			if (_type == "ack")
			{
				ackpId += pIdL + "+";
			}

			encoded << ackpId << this->stringify();
		}

		return encoded.str();
	}
	int SocketIOPacket::typeAsNumber()
	{
		int num = 0;
		std::vector<std::string>::iterator item = std::find(_types.begin(), _types.end(), _type);
		if (item != _types.end())
		{
			num = item - _types.begin();
		}
		return num;
	}
	std::string SocketIOPacket::typeForIndex(int index)
	{
		return _types.at(index);
	}

	void SocketIOPacket::addData(std::string data)
	{

		this->_args.push_back(data);

	}

	std::string SocketIOPacket::stringify()
	{
		//log("Before Stringify: %s", _args[0].c_str());
		std::string outS;
		if (_type == "message") {
			outS = _args[0];
		}
		else {

			rapidjson::StringBuffer s;
			rapidjson::Writer<rapidjson::StringBuffer> writer(s);

			writer.StartObject();
			writer.String("name");
			writer.String(_name.c_str());

			writer.String("args");

			writer.StartArray();

			for (int i = 0; i < _args.size(); i++) {
				writer.String(_args[i].c_str());
			}

			writer.EndArray();
			writer.EndObject();

			outS = s.GetString();

			//log("create args object: %s:", outS.c_str());
		}

		return outS;
	}

	SocketIOPacketV10x::SocketIOPacketV10x()
	{
		_separator = ":";
		_type = "";//message type
		_separator = "";//for stringify the object
		_endpointseperator = ",";
		_pId = "";//id message
		_ack = "";//
		_name = "";//event name
		_endpoint = "";//
		_types.push_back("disconnected");
		_types.push_back("connected");
		_types.push_back("heartbeat");
		_types.push_back("pong");
		_types.push_back("message");
		_types.push_back("upgrade");
		_types.push_back("noop");
		_typesMessage.push_back("connect");
		_typesMessage.push_back("disconnect");
		_typesMessage.push_back("event");
		_typesMessage.push_back("ack");
		_typesMessage.push_back("error");
		_typesMessage.push_back("binarevent");
		_typesMessage.push_back("binaryack");
	}

	int SocketIOPacketV10x::typeAsNumber()
	{
		int num = 0;
		std::vector<std::string>::iterator item = std::find(_typesMessage.begin(), _typesMessage.end(), _type);
		if (item != _typesMessage.end())
		{//it's a message
			num = item - _typesMessage.begin();
			num += 40;
		}
		else
		{
			item = std::find(_types.begin(), _types.end(), _type);
			num += item - _types.begin();
		}
		return num;
	}

	std::string SocketIOPacketV10x::stringify()
	{

		std::string outS;

		rapidjson::StringBuffer s;
		rapidjson::Writer<rapidjson::StringBuffer> writer(s);

		writer.StartArray();
		writer.String(_name.c_str());

		for (int i = 0; i < _args.size(); i++) {
			writer.String(_args[i].c_str());
		}

		writer.EndArray();

		outS = s.GetString();

		//log("create args object: %s:", outS.c_str());

		return outS;

	}

	SocketIOPacketV10x::~SocketIOPacketV10x()
	{
		_types.clear();
		_typesMessage.clear();
		_type = "";
		_pId = "";
		_name = "";
		_ack = "";
		_endpoint = "";
	}

	SocketIOPacket * SocketIOPacket::createPacketWithType(std::string type, SocketIOPacket::SocketIOVersion version)
	{
		SocketIOPacket *ret;
		switch (version)
		{
		case SocketIOPacket::V09x:
			ret = new SocketIOPacket;
			break;
		case SocketIOPacket::V10x:
			ret = new SocketIOPacketV10x;
			break;
		}
		ret->initWithType(type);
		return ret;
	}


	SocketIOPacket * SocketIOPacket::createPacketWithTypeIndex(int type, SocketIOPacket::SocketIOVersion version)
	{
		SocketIOPacket *ret;
		switch (version)
		{
		case SocketIOPacket::V09x:
			ret = new SocketIOPacket;
			break;
		case SocketIOPacket::V10x:
			return new SocketIOPacketV10x;
			break;
		}
		ret->initWithTypeIndex(type);
		return ret;
	}

	/**
	*  @brief The implementation of the socket.io connection
	*         Clients/endpoints may share the same impl to accomplish multiplexing on the same websocket
	*/
	class SIOClientImpl :
		public cocos2d::Ref,
		public WebSocket::Delegate
	{
	private:
		int _port, _heartbeat, _timeout;
		std::string _host, _sid, _uri;
		bool _connected;
		SocketIOPacket::SocketIOVersion _version;

		WebSocket *_ws;

		Map<std::string, SIOClient*> _clients;

	public:
		SIOClientImpl(const std::string& host, int port);
		virtual ~SIOClientImpl(void);

		static SIOClientImpl* create(const std::string& host, int port);

		virtual void onOpen(WebSocket* ws);
		virtual void onMessage(WebSocket* ws, const WebSocket::Data& data);
		virtual void onClose(WebSocket* ws);
		virtual void onError(WebSocket* ws, const WebSocket::ErrorCode& error);

		void connect();
		void disconnect();
		bool init();
		void handshake();
		void handshakeResponse(HttpClient *sender, HttpResponse *response);
		void openSocket();
		void heartbeat(float dt);

		SIOClient* getClient(const std::string& endpoint);
		void addClient(const std::string& endpoint, SIOClient* client);

		void connectToEndpoint(const std::string& endpoint);
		void disconnectFromEndpoint(const std::string& endpoint);

		void send(std::string endpoint, std::string s);
		void send(SocketIOPacket *packet);
		void emit(std::string endpoint, std::string eventname, std::string args);


	};


	//method implementations

	//begin SIOClientImpl methods
	SIOClientImpl::SIOClientImpl(const std::string& host, int port) :
		_port(port),
		_host(host),
		_connected(false)
	{
		std::stringstream s;
		s << host << ":" << port;
		_uri = s.str();

		_ws = nullptr;
	}

	SIOClientImpl::~SIOClientImpl()
	{
		if (_connected)
			disconnect();

		CC_SAFE_DELETE(_ws);
	}

	void SIOClientImpl::handshake()
	{
		log("SIOClientImpl::handshake() called");

		std::stringstream pre;
		pre << "http://" << _uri << "/socket.io/1/?EIO=2&transport=polling&b64=true";

		HttpRequest* request = new (std::nothrow) HttpRequest();
		request->setUrl(pre.str().c_str());
		request->setRequestType(HttpRequest::Type::GET);

		request->setResponseCallback(CC_CALLBACK_2(SIOClientImpl::handshakeResponse, this));
		request->setTag("handshake");

		log("SIOClientImpl::handshake() waiting");

		HttpClient::getInstance()->send(request);

		request->release();

		return;
	}

	void SIOClientImpl::handshakeResponse(HttpClient *sender, HttpResponse *response)
	{
		log("SIOClientImpl::handshakeResponse() called");

		if (0 != strlen(response->getHttpRequest()->getTag()))
		{
			log("%s completed", response->getHttpRequest()->getTag());
		}

		long statusCode = response->getResponseCode();
		char statusString[64] = {};
		sprintf(statusString, "HTTP Status Code: %ld, tag = %s", statusCode, response->getHttpRequest()->getTag());
		log("response code: %ld", statusCode);

		if (!response->isSucceed())
		{
			log("SIOClientImpl::handshake() failed");
			log("error buffer: %s", response->getErrorBuffer());

			for (auto iter = _clients.begin(); iter != _clients.end(); ++iter)
			{
				iter->second->getDelegate()->onError(iter->second, response->getErrorBuffer());
			}

			return;
		}

		log("SIOClientImpl::handshake() succeeded");

		std::vector<char> *buffer = response->getResponseData();
		std::stringstream s;
		s.str("");

		for (unsigned int i = 0; i < buffer->size(); i++)
		{
			s << (*buffer)[i];
		}

		log("SIOClientImpl::handshake() dump data: %s", s.str().c_str());

		std::string res = s.str();
		std::string sid = "";
		int heartbeat = 0, timeout = 0;

		if (res.at(res.size() - 1) == '}') {

			log("SIOClientImpl::handshake() Socket.IO 1.x detected");
			_version = SocketIOPacket::V10x;
			// sample: 97:0{"sid":"GMkL6lzCmgMvMs9bAAAA","upgrades":["websocket"],"pingInterval":25000,"pingTimeout":60000}

			int a, b;
			a = res.find('{');
			std::string temp = res.substr(a, res.size() - a);

			// find the sid
			a = temp.find(":");
			b = temp.find(",");

			sid = temp.substr(a + 2, b - (a + 3));

			temp = temp.erase(0, b + 1);

			// chomp past the upgrades
			a = temp.find(":");
			b = temp.find(",");

			temp = temp.erase(0, b + 1);

			// get the pingInterval / heartbeat
			a = temp.find(":");
			b = temp.find(",");

			std::string heartbeat_str = temp.substr(a + 1, b - a);
			heartbeat = atoi(heartbeat_str.c_str()) / 1000;
			temp = temp.erase(0, b + 1);

			// get the timeout
			a = temp.find(":");
			b = temp.find("}");

			std::string timeout_str = temp.substr(a + 1, b - a);
			timeout = atoi(timeout_str.c_str()) / 1000;
			log("done parsing 1.x");

		}
		else {

			log("SIOClientImpl::handshake() Socket.IO 0.9.x detected");
			_version = SocketIOPacket::V09x;
			// sample: 3GYzE9md2Ig-lm3cf8Rv:60:60:websocket,htmlfile,xhr-polling,jsonp-polling
			size_t pos = 0;

			pos = res.find(":");
			if (pos != std::string::npos)
			{
				sid = res.substr(0, pos);
				res.erase(0, pos + 1);
			}

			pos = res.find(":");
			if (pos != std::string::npos)
			{
				heartbeat = atoi(res.substr(pos + 1, res.size()).c_str());
			}

			pos = res.find(":");
			if (pos != std::string::npos)
			{
				timeout = atoi(res.substr(pos + 1, res.size()).c_str());
			}

		}

		_sid = sid;
		_heartbeat = heartbeat;
		_timeout = timeout;

		openSocket();

		return;

	}

	void SIOClientImpl::openSocket()
	{
		log("SIOClientImpl::openSocket() called");

		std::stringstream s;

		switch (_version)
		{
		case SocketIOPacket::V09x:
			s << _uri << "/socket.io/1/websocket/" << _sid;
			break;
		case SocketIOPacket::V10x:
			s << _uri << "/socket.io/1/websocket/?EIO=2&transport=websocket&sid=" << _sid;
			break;
		}

		_ws = new (std::nothrow) WebSocket();
		if (!_ws->init(*this, s.str()))
		{
			CC_SAFE_DELETE(_ws);
		}

		return;
	}

	bool SIOClientImpl::init()
	{
		log("SIOClientImpl::init() successful");
		return true;
	}

	void SIOClientImpl::connect()
	{
		this->handshake();
	}

	void SIOClientImpl::disconnect()
	{
		if (_ws->getReadyState() == WebSocket::State::OPEN)
		{
			std::string s, endpoint;
			s = "";
			endpoint = "";

			if (_version == SocketIOPacket::V09x)
				s = "0::" + endpoint;
			else
				s = "41" + endpoint;
			_ws->send(s);
		}

		Director::getInstance()->getScheduler()->unscheduleAllForTarget(this);

		_ws->close();

		_connected = false;

		SocketIO::getInstance()->removeSocket(_uri);
	}

	SIOClientImpl* SIOClientImpl::create(const std::string& host, int port)
	{
		SIOClientImpl *s = new (std::nothrow) SIOClientImpl(host, port);

		if (s && s->init())
		{
			return s;
		}

		return nullptr;
	}

	SIOClient* SIOClientImpl::getClient(const std::string& endpoint)
	{
		return _clients.at(endpoint);
	}

	void SIOClientImpl::addClient(const std::string& endpoint, SIOClient* client)
	{
		_clients.insert(endpoint, client);
	}

	void SIOClientImpl::connectToEndpoint(const std::string& endpoint)
	{
		SocketIOPacket *packet = SocketIOPacket::createPacketWithType("connect", _version);
		packet->setEndpoint(endpoint);
		this->send(packet);
	}

	void SIOClientImpl::disconnectFromEndpoint(const std::string& endpoint)
	{
		_clients.erase(endpoint);

		if (_clients.empty() || endpoint == "/")
		{
			log("SIOClientImpl::disconnectFromEndpoint out of endpoints, checking for disconnect");

			if (_connected)
				this->disconnect();
		}
		else
		{
			std::string path = endpoint == "/" ? "" : endpoint;

			std::string s = "0::" + path;

			_ws->send(s);
		}
	}

	void SIOClientImpl::heartbeat(float dt)
	{
		SocketIOPacket *packet = SocketIOPacket::createPacketWithType("heartbeat", _version);

		this->send(packet);

		log("Heartbeat sent");
	}


	void SIOClientImpl::send(std::string endpoint, std::string s)
	{
		switch (_version) {
		case SocketIOPacket::V09x:
		{
			SocketIOPacket *packet = SocketIOPacket::createPacketWithType("message", _version);
			packet->setEndpoint(endpoint);
			packet->addData(s);
			this->send(packet);
			break;
		}
		case SocketIOPacket::V10x:
		{
			this->emit(endpoint, "message", s);
			break;
		}
		}
	}

	void SIOClientImpl::send(SocketIOPacket *packet)
	{
		std::string req = packet->toString();
		if (_connected)
		{
			//log("-->SEND:%s", req.data());
			_ws->send(req.data());
		}
		else
			log("Cant send the message (%s) because disconnected", req.c_str());
	}

	void SIOClientImpl::emit(std::string endpoint, std::string eventname, std::string args)
	{
		//log("Emitting event \"%s\"", eventname.c_str());
		SocketIOPacket *packet = SocketIOPacket::createPacketWithType("event", _version);
		packet->setEndpoint(endpoint == "/" ? "" : endpoint);
		packet->setEvent(eventname);
		packet->addData(args);
		this->send(packet);
	}

	void SIOClientImpl::onOpen(WebSocket* ws)
	{
		_connected = true;

		SocketIO::getInstance()->addSocket(_uri, this);

		if (_version == SocketIOPacket::V10x)
		{
			std::string s = "5";//That's a ping https://github.com/Automattic/engine.io-parser/blob/1b8e077b2218f4947a69f5ad18be2a512ed54e93/lib/index.js#L21
			_ws->send(s.data());
		}

		Director::getInstance()->getScheduler()->schedule(CC_SCHEDULE_SELECTOR(SIOClientImpl::heartbeat), this, (_heartbeat * .9f), false);

		for (auto iter = _clients.begin(); iter != _clients.end(); ++iter)
		{
			iter->second->onOpen();
		}

		log("SIOClientImpl::onOpen socket connected!");
	}

	void SIOClientImpl::onMessage(WebSocket* ws, const WebSocket::Data& data)
	{
		//log("SIOClientImpl::onMessage received: %s", data.bytes);

		std::string payload = data.bytes;
		int control = atoi(payload.substr(0, 1).c_str());
		payload = payload.substr(1, payload.size() - 1);

		SIOClient *c = nullptr;

		switch (_version)
		{
		case SocketIOPacket::V09x:
		{
			std::string msgid, endpoint, s_data, eventname;

			size_t pos, pos2;

			pos = payload.find(":");
			if (pos != std::string::npos) {
				payload.erase(0, pos + 1);
			}

			pos = payload.find(":");
			if (pos != std::string::npos) {
				msgid = atoi(payload.substr(0, pos + 1).c_str());
			}
			payload.erase(0, pos + 1);

			pos = payload.find(":");
			if (pos != std::string::npos)
			{
				endpoint = payload.substr(0, pos);
				payload.erase(0, pos + 1);
			}
			else
			{
				endpoint = payload;
			}

			if (endpoint == "") endpoint = "/";

			c = getClient(endpoint);

			s_data = payload;

			if (c == nullptr) log("SIOClientImpl::onMessage client lookup returned nullptr");

			switch (control)
			{
			case 0:
				log("Received Disconnect Signal for Endpoint: %s\n", endpoint.c_str());
				disconnectFromEndpoint(endpoint);
				c->fireEvent("disconnect", payload);
				break;
			case 1:
				log("Connected to endpoint: %s \n", endpoint.c_str());
				if (c) {
					c->onConnect();
					c->fireEvent("connect", payload);
				}
				break;
			case 2:
				log("Heartbeat received\n");
				break;
			case 3:
				log("Message received: %s \n", s_data.c_str());
				if (c) c->getDelegate()->onMessage(c, s_data);
				if (c) c->fireEvent("message", s_data);
				break;
			case 4:
				log("JSON Message Received: %s \n", s_data.c_str());
				if (c) c->getDelegate()->onMessage(c, s_data);
				if (c) c->fireEvent("json", s_data);
				break;
			case 5:
				log("Event Received with data: %s \n", s_data.c_str());

				if (c)
				{
					eventname = "";
					pos = s_data.find(":");
					pos2 = s_data.find(",");
					if (pos2 > pos)
					{
						eventname = s_data.substr(pos + 2, pos2 - (pos + 3));
						s_data = s_data.substr(pos2 + 9, s_data.size() - (pos2 + 11));
					}
					//c->fireEvent(eventname, payload); //Here for return all string client received like: {"user_id":2,"room_id":1,"team_id":2,"uuid":"5554094e7e606"}
					c->fireEvent(eventname, s_data); //here for return  only JSON object like: {"name":"connect_select_team_end","args":[{"user_id":2,"room_id":1,"team_id":2,"uuid":"5554094e7e606"}]}
				}

				break;
			case 6:
				log("Message Ack\n");
				break;
			case 7:
				log("Error\n");
				//if (c) c->getDelegate()->onError(c, s_data);
				if (c) c->fireEvent("error", s_data);
				break;
			case 8:
				log("Noop\n");
				break;
			}
		}
		break;
		case SocketIOPacket::V10x:
		{
			switch (control)
			{
			case 0:
				log("Not supposed to receive control 0 for websocket");
				log("That's not good");
				break;
			case 1:
				log("Not supposed to receive control 1 for websocket");
				break;
			case 2:
				log("Ping received, send pong");
				payload = "3" + payload;
				_ws->send(payload.c_str());
				break;
			case 3:
				log("Pong received");
				if (payload == "probe")
				{
					log("Request Update");
					_ws->send("5");
				}
				break;
			case 4:

				const char second = payload.at(0);
				int control2 = atoi(&second);
				//log("Message code: [%i]", control);

				SocketIOPacket *packetOut = SocketIOPacket::createPacketWithType("event", _version);
				std::string endpoint = "";

				int a = payload.find("/");
				int b = payload.find("[");

				if (b != std::string::npos) {
					if (a != std::string::npos && a < b) {
						//we have an endpoint and a payload
						endpoint = payload.substr(a, b - (a + 1));
					}
				}
				else if (a != std::string::npos) {
					//we have an endpoint with no payload
					endpoint = payload.substr(a, payload.size() - a);
				}

				// we didn't find and endpoint and we are in the default namespace
				if (endpoint == "") endpoint = "/";

				packetOut->setEndpoint(endpoint);

				c = getClient(endpoint);

				payload = payload.substr(1);

				if (endpoint != "/") payload = payload.substr(endpoint.size());
				if (endpoint != "/" && payload != "") payload = payload.substr(1);

				switch (control2)
				{
				case 0:
					log("Socket Connected");
					if (c) {
						c->onConnect();
						c->fireEvent("connect", payload);
					}
					break;
				case 1:
					log("Socket Disconnected");
					disconnectFromEndpoint(endpoint);
					c->fireEvent("disconnect", payload);
					break;
				case 2:
				{
					//log("Event Received (%s)", payload.c_str());

					int a = payload.find("\"");
					int b = payload.substr(a + 1).find("\"");

					std::string eventname = payload.substr(a + 1, b - a + 1);
					//log("event name %s between %i and %i", eventname.c_str(), a, b);

					payload = payload.substr(b + 4, payload.size() - (b + 5));

					if (c) c->fireEvent(eventname, payload);
					if (c) c->getDelegate()->onMessage(c, payload);

				}
				break;
				case 3:
					log("Message Ack");
					break;
				case 4:
					log("Error");
					if (c) c->fireEvent("error", payload);
					break;
				case 5:
					log("Binary Event");
					break;
				case 6:
					log("Binary Ack");
					break;
				}
			}
			break;
		case 5:
			log("Upgrade required");
			break;
		case 6:
			log("Noop\n");
			break;
		}
		break;
		}

		return;
	}

	void SIOClientImpl::onClose(WebSocket* ws)
	{
		if (!_clients.empty())
		{
			for (auto iter = _clients.begin(); iter != _clients.end(); ++iter)
			{
				iter->second->socketClosed();
			}
		}

		this->release();
	}

	void SIOClientImpl::onError(WebSocket* ws, const WebSocket::ErrorCode& error)
	{
		// 		log("Websocket error received: %d", error);
	}

	//begin SIOClient methods
	SIOClient::SIOClient(const std::string& host, int port, const std::string& path, SIOClientImpl* impl, SocketIO::SIODelegate& delegate)
		: _port(port)
		, _host(host)
		, _path(path)
		, _connected(false)
		, _socket(impl)
		, _delegate(&delegate)
	{

	}

	SIOClient::~SIOClient(void)
	{
		if (_connected)
		{
			_socket->disconnectFromEndpoint(_path);
		}
	}

	void SIOClient::onOpen()
	{
		if (_path != "/")
		{
			_socket->connectToEndpoint(_path);
		}
	}

	void SIOClient::onConnect()
	{
		_connected = true;
	}

	void SIOClient::send(std::string s)
	{
		if (_connected)
		{
			_socket->send(_path, s);
		}
		else
		{
			_delegate->onError(this, "Client not yet connected");
		}

	}

	void SIOClient::emit(std::string eventname, std::string args)
	{
		if (_connected)
		{
			_socket->emit(_path, eventname, args);
		}
		else
		{
			_delegate->onError(this, "Client not yet connected");
		}

	}

	void SIOClient::disconnect()
	{
		_connected = false;

		_socket->disconnectFromEndpoint(_path);

		this->release();
	}

	void SIOClient::socketClosed()
	{
		_connected = false;

		_delegate->onClose(this);

		this->release();
	}

	void SIOClient::on(const std::string& eventName, SIOEvent e)
	{
		_eventRegistry[eventName] = e;
	}

	void SIOClient::fireEvent(const std::string& eventName, const std::string& data)
	{
		//log("SIOClient::fireEvent called with event name: %s and data: %s", eventName.c_str(), data.c_str());

		_delegate->fireEventToScript(this, eventName, data);

		if (_eventRegistry[eventName])
		{
			SIOEvent e = _eventRegistry[eventName];

			e(this, data);

			return;
		}

		log("SIOClient::fireEvent no native event with name %s found", eventName.c_str());
	}

	//begin SocketIO methods
	SocketIO *SocketIO::_inst = nullptr;

	SocketIO::SocketIO()
	{
	}

	SocketIO::~SocketIO(void)
	{
	}

	SocketIO* SocketIO::getInstance()
	{
		if (nullptr == _inst)
			_inst = new (std::nothrow) SocketIO();

		return _inst;
	}

	void SocketIO::destroyInstance()
	{
		CC_SAFE_DELETE(_inst);
	}

	SIOClient* SocketIO::connect(SocketIO::SIODelegate& delegate, const std::string& uri)
	{

		return SocketIO::connect(uri, delegate);

	}

	SIOClient* SocketIO::connect(const std::string& uri, SocketIO::SIODelegate& delegate)
	{
		std::string host = uri;
		int port = 0;
		size_t pos = 0;

		pos = host.find("//");
		if (pos != std::string::npos)
		{
			host.erase(0, pos + 2);
		}

		pos = host.find(":");
		if (pos != std::string::npos)
		{
			port = atoi(host.substr(pos + 1, host.size()).c_str());
		}

		pos = host.find("/", 0);
		std::string path = "/";
		if (pos != std::string::npos)
		{
			path += host.substr(pos + 1, host.size());
		}

		pos = host.find(":");
		if (pos != std::string::npos)
		{
			host.erase(pos, host.size());
		}
		else if ((pos = host.find("/")) != std::string::npos)
		{
			host.erase(pos, host.size());
		}

		std::stringstream s;
		s << host << ":" << port;

		SIOClientImpl* socket = nullptr;
		SIOClient *c = nullptr;

		socket = SocketIO::getInstance()->getSocket(s.str());

		if (socket == nullptr)
		{
			//create a new socket, new client, connect
			socket = SIOClientImpl::create(host, port);

			c = new (std::nothrow) SIOClient(host, port, path, socket, delegate);

			socket->addClient(path, c);

			socket->connect();
		}
		else
		{
			//check if already connected to endpoint, handle
			c = socket->getClient(path);

			if (c == nullptr)
			{
				c = new (std::nothrow) SIOClient(host, port, path, socket, delegate);

				socket->addClient(path, c);

				socket->connectToEndpoint(path);
			}
		}

		return c;
	}

	SIOClientImpl* SocketIO::getSocket(const std::string& uri)
	{
		return _sockets.at(uri);
	}

	void SocketIO::addSocket(const std::string& uri, SIOClientImpl* socket)
	{
		_sockets.insert(uri, socket);
	}

	void SocketIO::removeSocket(const std::string& uri)
	{
		_sockets.erase(uri);
	}

}

NS_CC_END

