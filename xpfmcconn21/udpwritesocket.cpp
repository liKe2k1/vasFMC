#include "udpwritesocket.h"

UDPWriteSocket::UDPWriteSocket()
{
#ifdef WIN32
    // Start the Winsocket API
    WSADATA wsa;

    // Load Winsock 2.2
    if ( WSAStartup (MAKEWORD(2, 2), &wsa) != 0 )
    {
        fprintf(stderr, "WSAStartup failed");
        exit (1);
    }
#endif

    // Get a socket
    if ( (sockId = socket(PF_INET, SOCK_DGRAM, 0) ) < 0)
    {
        PERROR ("Socket failed");
        exit (1);
    }
}

UDPWriteSocket::~UDPWriteSocket()
{
    delete[] destinationAddr;
    // Close socket
    close (sockId);
#ifdef WIN_32
    WSACleanup();
#endif
}

void UDPWriteSocket::configure(const std::string& host, int port)
{
    destinationAddr = new char [host.size()+1];
    strcpy(destinationAddr, host.c_str());
    destinationPort = port;
    printf ("Host %s:%i\n", destinationAddr, destinationPort);

    // Allow this socket to emit broadcasts.
    socketOption_t broadcast = 1;
    if ( setsockopt(sockId, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof (broadcast)) == -1 )
    {
        PERROR ("setsockopt BROADCAST");
        exit (1);
    }

    /*
    // If we want to send on a given port set up an address structure.
    struct sockaddr_in myAddr;
    memset (&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons (MY_PORT_ID);
    myAddr.sin_addr.s_addr = htonl (INADDR_ANY);

    // Now bind to that port.
    if ( bind(sockId, (struct sockaddr *) &myAddr, sizeof(myAddr)) < 0 )
    {
        printf ("Bind failed with errno: %d\n", errno);
        PERROR ("Bind failed");
        exit (1);
    }
    */

    /* // Set another multicast interface.
    if ( setsockopt(sockId, IPPROTO_IP, IP_MULTICAST_IF, &multicastIf, sizeof (multicastIf)) < 0 )
    {
        PERROR("setsockopt LOOPBACK");
        exit (1);
    }
    */

    /* // Disable the loopback.
    socketOption_t loopback = 0;
    if ( setsockopt(sockId, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof (loopback)) < 0 )
    {
        PERROR("setsockopt LOOPBACK");
        exit (1);
    }
    */

    // Set the time to live/hop count.
    socketOption_t ttl = 4;
    if ( setsockopt(sockId, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof (ttl)) < 0 )
    {
        PERROR("setsockopt TTL");
        exit (1);
    }

    // Set up address and port of host.
    memset (&hostAddr, 0, sizeof(hostAddr));
    hostAddr.sin_family = AF_INET;
    hostAddr.sin_port = htons (destinationPort);
    hostAddr.sin_addr.s_addr = inet_addr (destinationAddr);
}

long UDPWriteSocket::write(const void* data, size_t size)
{
    long bytes_written =
#ifdef WIN_32
            sendto (sockId, (const char*)data, size, 0, (struct sockaddr *)&hostAddr, sizeof(hostAddr));
#else
            sendto (sockId, data, size, 0, (struct sockaddr *)&hostAddr, sizeof(hostAddr));
#endif
    if ( bytes_written < 0 )
    {
        PERROR ("Sendto2 failed");
        exit (1);
    }
    return bytes_written;
}
