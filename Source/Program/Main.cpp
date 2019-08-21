#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <sstream>
#include <queue>
#include <mutex>

#include <asio.hpp>

constexpr const bool c_Verbose = true;

void Log(const std::string_view& p_Message)
{
    if constexpr (c_Verbose)
    {
        std::cout << p_Message << "\r\n";
    }
}

void StringToLower(std::string& string)
{
    for (auto& c : string)
    {
        c = std::tolower(c);
    }
}

#define DefineIntegralTypeCheck(p_FunctionName, p_IntegralType)         \
    template<class Type>                                \
    struct p_FunctionName                               \
    {                                                   \
        static const bool value = false;                \
    };                                                  \
                                                        \
    template<>                                          \
    struct p_FunctionName<p_IntegralType>              \
    {                                                   \
        static const bool value = true;                 \
    }

DefineIntegralTypeCheck(TypeIsInt, int);
DefineIntegralTypeCheck(TypeIsLong, long);
DefineIntegralTypeCheck(TypeIsLongLong, long long);

template<typename OutputType>
const bool TryParse(const std::string_view& p_String,
                    OutputType& p_Output)
{
    bool l_result {true};
    if constexpr (std::is_integral<OutputType>::value)
    {
        if (!p_String.empty())
        {
            try
            {
                if constexpr (TypeIsInt<OutputType>::value)
                {
                    p_Output = std::stoi(p_String.data());
                }
                else if constexpr (TypeIsLong<OutputType>::value)
                {
                    p_Output = std::stol(p_String);
                }
                else if constexpr (TypeIsLongLong<OutputType>::value)
                {
                    p_Output = std::stoll(p_String);
                }
                else
                {
                    l_result = false;
                    Log("Parse Failed, Parameter type was not an integer or long type");
                }
            }
            catch ([[maybe_unused]] const std::invalid_argument& l_InvalidArgumentException)
            {
                l_result = false;
                Log("Parse Failed, invalid argument");
            }
            catch ([[maybe_unused]] const std::out_of_range& l_OutOfRangeException)
            {
                l_result = false;
                Log("Parse Failed, out of range");
            }
        }
        else
        {
            l_result = false;
            Log("Parse Failed, empty string provided");
        }
    }
    else
    {
        l_result = false;
        Log("Parse Failed, Output Parse Type is not an integral");
    }
    return l_result;
}


constexpr const char* c_ServerString = "server";
constexpr const char* c_ClientString = "client";
constexpr const char* c_PortString = "port";
constexpr const char* c_AddressString = "address";

using PortNumber = unsigned short;

class ApplicationConfig;
using ApplicationConfigPtr = std::unique_ptr<ApplicationConfig>;
class ApplicationConfig
{
public:
    enum class Mode
    {
        Unknown,
        Server,
        Client
    };

    static constexpr const char* cs_UnknownModeString = "Unknown";
    static constexpr const char* cs_ServerModeString = "Server";
    static constexpr const char* cs_ClientModeString = "Client";

    static constexpr const char* GetModeString(const Mode p_Mode)
    {
        switch (p_Mode)
        {
        case Mode::Server:  return cs_ServerModeString;

        case Mode::Client:  return cs_ClientModeString;

        default: return cs_UnknownModeString;
        }
    }

    const Mode m_Mode;
    const std::string m_Address;
    const PortNumber m_Port;

    asio::ip::tcp::endpoint GetClientEndPoint() const
    {
        return asio::ip::tcp::endpoint(asio::ip::make_address_v4(m_Address), m_Port);
    }

    asio::ip::tcp::endpoint GetServerEndPoint() const
    {
        return asio::ip::tcp::endpoint(asio::ip::tcp::v4(), m_Port);
    }

    static std::optional<ApplicationConfigPtr> Create(const std::vector<std::string>& p_ApplicationParameters)
    {
        std::optional<ApplicationConfigPtr> l_result;
        if (!p_ApplicationParameters.empty())
        {
            bool l_AddressFound {false};
            std::string l_Address {c_DefaultAddressString};

            bool l_PortFound {false};
            PortNumber l_Port {c_DefaultPortNumber};

            bool l_ModeFound {false};
            Mode l_Mode {Mode::Unknown};

            for (auto l_itr = p_ApplicationParameters.cbegin();
                 l_itr != p_ApplicationParameters.cend();
                 ++l_itr)
            {
                auto l_next = (l_itr + 1);

                // Always need one more to look-up a valid
                if (l_next != p_ApplicationParameters.end())
                {
                    std::string_view l_Current {*l_itr};
                    if (!l_AddressFound && (l_Current == c_AddressString))
                    {
                        l_Address = *l_next;
                        l_AddressFound = true;
                    }

                    if (!l_PortFound && (l_Current == c_PortString))
                    {
                        int l_tempPort {0};
                        l_PortFound = TryParse<int>(*l_next, l_tempPort);
                        if (l_PortFound)
                        {
                            // Truncation is fine, should be in range anyway
                            l_Port = static_cast<PortNumber>(l_tempPort);
                        }
                    }

                    if (!l_ModeFound && (l_Current == c_ClientString))
                    {
                        l_ModeFound = true;
                        l_Mode = Mode::Client;
                    }

                    if (!l_ModeFound && (l_Current == c_ServerString))
                    {
                        l_ModeFound = true;
                        l_Mode = Mode::Server;
                    }
                }
            }

            if (l_ModeFound)
            {
                if (!l_AddressFound)
                {
                    Log("Using Default Address");
                }
                if (!l_PortFound)
                {
                    Log("Using Default Port");
                }

                l_result.emplace(
                    std::move(
                        std::unique_ptr<ApplicationConfig>(
                            new ApplicationConfig(
                                l_Mode,
                                l_Address,
                                l_Port))));
            }
            else
            {
                Log("Mode Is required");
            }
        }
        return l_result;
    }

private:
    static constexpr const char* c_DefaultAddressString = "127.0.0.1";
    static constexpr const PortNumber c_DefaultPortNumber = 7500;

    ApplicationConfig() = delete;
    ApplicationConfig(const ApplicationConfig&) = delete;
    ApplicationConfig(ApplicationConfig&&) = delete;
    ApplicationConfig& operator=(const ApplicationConfig&) = delete;
    ApplicationConfig& operator=(ApplicationConfig&&) = delete;

    ApplicationConfig(const Mode& p_Mode,
                      const std::string_view& p_Address,
                      const PortNumber& p_PortNumber)
        : m_Mode(p_Mode)
        , m_Address(p_Address)
        , m_Port(p_PortNumber)
    {
        if constexpr (c_Verbose)
        {
            std::ostringstream l_oss;
            l_oss << "Application Config [" << GetModeString(m_Mode) << " : " << m_Address << " : " << m_Port << "]";
            Log(l_oss.str());
        }
    }
};

void Usage()
{
    std::cout << "Usage:\r\n";
    std::cout << "\tStandaloneAsio [mode] address [ip] port [number]\r\n";
    std::cout << "\t\t[Mode]  server | client\r\n";
    std::cout << "\t\taddress required to tell the server which address to bind or client to connect\r\n";
    std::cout << "\t\t[ip]  an IPv4 Address\r\n";
    std::cout << "\t\tport required to tell the server or client to which port to bind or connect\r\n";
    std::cout << "\t\t[number] the number for the port\r\n\r\n";
    std::cout << "Example: StandaloneAsio server address 127.0.0.1 port 8000\r\n";
    std::cout << "         StandaloneAsio client address 192.168.0.1 port 3400\r\n";
    std::cout << std::flush;
}

class Session
{
private:
    asio::ip::tcp::socket m_Socket;
    asio::mutable_buffer m_ReadBuffer;
    std::atomic_bool m_MasterExit {false};
    std::atomic_bool m_ReadWait {false};

    std::mutex m_QueueMutex;
    std::queue<std::string> m_Messages;

public:
    void StartRead()
    {
        if (m_Socket.is_open() && !m_ReadWait)
        {
            m_ReadWait = true;
            asio::async_read(
                m_Socket,
                m_ReadBuffer,
                [this](const asio::error_code& p_Error, std::size_t p_BytesRead)
            {
                m_ReadWait = false;
                if (!p_Error)
                {
                    if (m_ReadBuffer.size() > 0)
                    {
                        std::scoped_lock<std::mutex> l_Lock(m_QueueMutex);
                        std::string_view l_View(reinterpret_cast<char*>(m_ReadBuffer.data()), m_ReadBuffer.size());
                        std::string l_Copy(l_View);
                        Log("Received [" + l_Copy + "]");
                        m_Messages.push(std::move(l_Copy));
                    }
                    else
                    {
                        Log("Empty Message Received");
                    }
                }
                if (!m_MasterExit)
                {
                    StartRead();
                }
            });
        }
    }

    Session(const Session& p_OtherSession) = default;

    Session() = delete;

    Session(asio::ip::tcp::socket& p_Socket)
        : m_Socket(std::move(p_Socket))
    {
        StartRead();
    }

    void Close()
    {
        if (m_Socket.is_open())
        {
            m_Socket.close();
        }
    }

    ~Session()
    {
        m_MasterExit = true;
        Close();
    }

    const bool InReadWait() const
    {
        bool l_result {m_ReadWait.load()};
        return l_result;
    }
};

//class Client
//{
//private:
//    asio::ip::tcp::socket m_Connection;
//    std::atomic_bool m_MasterExit{ false };
//
//    Session* m_Session{ nullptr };
//
//public:
//    Client(asio::io_context& p_Context, const ApplicationConfig* p_Config)
//        :
//        m_Connection(p_Context)
//    {
//        asio::async_connect(
//            m_Connection,
//            p_Config->GetClientEndPoint(),
//            [this](std::error_code& p_ErrorCode, asio::ip::tcp::endpoint& p_RemoteAddress)
//            {
//                if (!p_ErrorCode)
//                {
//                    m_Session = new Session(m_Connection);
//                }
//            });
//    }
//
//    Session const* GetSession() const { return m_Session; }
//
//    const bool IsOpen() const { return m_Connection.is_open(); }
//
//};

class Server
{
private:
    uint64_t m_ConnectionsCount {0};

    asio::ip::tcp::acceptor m_Acceptor;

    mutable std::mutex m_QueueMutex;
    std::queue<asio::ip::tcp::socket> m_ConnectedSockets;
    std::atomic_bool m_MasterExit {false};

    void StartAccept()
    {
        Log("Server Waiting...");
        m_Acceptor.async_accept(
            [this](const std::error_code& p_Error, asio::ip::tcp::socket p_IncomingSocket)
        {
            ++m_ConnectionsCount;
            Log("Server received connection [" + std::to_string(m_ConnectionsCount) + "...");
            if (!p_Error)
            {
                std::scoped_lock<std::mutex> l_Lock(m_QueueMutex);
                m_ConnectedSockets.push(std::move(p_IncomingSocket));
            }
            else
            {
                Log("Server Error... [" + std::to_string(p_Error.value()) + "]");
            }
            if (!m_MasterExit)
            {
                StartAccept();
            }
        });
    }

public:
    Server() = delete;
    Server(asio::io_context& p_Context,
           const ApplicationConfig* p_Config)
        : m_Acceptor(p_Context, p_Config->GetServerEndPoint())
    {
        StartAccept();
    }

    ~Server()
    {
        m_MasterExit = true;
        m_Acceptor.close();
    }

    const bool ConnectionWaiting() const
    {
        std::scoped_lock<std::mutex> l_Lock(m_QueueMutex);
        return !m_ConnectedSockets.empty();
    }

    [[nodiscard]] std::optional<asio::ip::tcp::socket> GetNextConnection()
    {
        std::optional<asio::ip::tcp::socket> l_Socket;

        {
            std::scoped_lock<std::mutex> l_Lock(m_QueueMutex);
            l_Socket.emplace(std::move(m_ConnectedSockets.front()));
            m_ConnectedSockets.pop();
        }

        return l_Socket;
    }
};

void RunServer(const ApplicationConfig* const p_Config)
{
    if (p_Config != nullptr)
    {
        asio::io_context l_IOContext(1);

        Server l_Server(l_IOContext, p_Config);

        std::vector<Session*> l_Sessions;

        uint64_t l_Ops {0};

        do
        {
            l_Ops += l_IOContext.run_one();
            Log("Server Running [" + std::to_string(l_Ops) + "] operations");
            if (l_Server.ConnectionWaiting())
            {
                auto l_Connection = l_Server.GetNextConnection();
                if (l_Connection.has_value())
                {
                    auto l_Session = new Session(l_Connection.value());
                    l_Sessions.push_back(l_Session);
                }
            }
        }
        while (l_Ops < 100);

        for (auto& session : l_Sessions)
        {
            delete session;
        }
        l_Sessions.clear();
    }
    else
    {
        Log("Missing Config");
    }
}

void RunClient(const ApplicationConfig* const p_Config)
{
    if (p_Config != nullptr)
    {
    }
    else
    {
        Log("Missing Config");
    }
}

int main(int p_argc,
         char** p_argv)
{
    std::vector<std::vector<int>> numbers {{1, 2, 3}, {1, 2, 3, 4, 5}, {1, 2, 3, 4, 5, 6, 7, 8}};

    for (const auto& numberList : numbers)
    {
        for (const auto& number : numberList)
        {
            std::cout << number << " ";
        }
        std::cout << "\r\n";
    }
    std::cout << "\r\n";

    for (auto& numberList : numbers)
    {
        numberList.erase(std::remove(numberList.begin(), numberList.end(), 2), numberList.end());
    }

    for (const auto& numberList : numbers)
    {
        for (const auto& number : numberList)
        {
            std::cout << number << " ";
        }
        std::cout << "\r\n";
    }
    std::cout << "\r\n";


    std::vector<std::string> l_Params;
    for (int i {0}; i < p_argc; ++i)
    {
        std::string l_param {p_argv[i]};
        StringToLower(l_param);
        l_Params.push_back(l_param);
    }
    auto l_Config = ApplicationConfig::Create(l_Params);

    if (l_Config.has_value())
    {
        switch (l_Config.value()->m_Mode)
        {
        case ApplicationConfig::Mode::Server:   RunServer(l_Config.value().get());        break;
        case ApplicationConfig::Mode::Client:   RunClient(l_Config.value().get());        break;
        }
    }
    else
    {
        Log("No Configuration provided");
        Usage();
    }
}