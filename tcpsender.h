#ifndef TCPSENDER_H
#define TCPSENDER_H

#include <QDebug>
#include <QTimer>
#include <QObject>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <ctime>
#include <vector>
#include <pthread.h>

#define SERVERPORT 8887
#define SERVERIP   "192.168.8.122"

#define STATE_CONNECTION_CLOSED  0x00
#define STATE_WAITING_FOR_JOBS   0x01
#define STATE_WAITING_FOR_REPLY  0x02

#define SEND_MODE_WITH_REPLY 0x01
#define SEND_MODE_WITHOUT_REPLY 0x02

#define REPLY_DELAY_INTERVAL_IN_MS    100

// inherit qobject...
class TCPSender : public QObject
{
    Q_OBJECT

private:
    typedef struct job_type {
    char*            data_ptr;
    uint32_t*        data_length_ptr;
    uint8_t          mode;
    }   job_type;

    int                      clientSock;
    pthread_t                thread_send;
    uint8_t                  thread_state;
    bool*                    start_flag;
    pthread_mutex_t          job_list_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t          thread_state_mutex = PTHREAD_MUTEX_INITIALIZER;
    QTimer*                  m_replyDelayTimer;
    std::vector<job_type>    job_queue;

    void startMDelayTimer(){
        m_replyDelayTimer->start();
    }
    void stopMDelayTimer() {
        m_replyDelayTimer->stop();
    }

public slots:
    void messageDelayExpired() {
        qCritical()<<"Message didnt arrive in time";
    }


public:
    TCPSender(QObject *parent) : QObject(parent)
    {
        clientSock = - 1;
        thread_state = STATE_WAITING_FOR_JOBS;
        m_replyDelayTimer = new QTimer();
        m_replyDelayTimer->setSingleShot(true);
        m_replyDelayTimer->setInterval(REPLY_DELAY_INTERVAL_IN_MS);

        QObject::connect( m_replyDelayTimer,SIGNAL(timeout()), this, SLOT(messageDelayExpired()));

        if (pthread_create(&this->thread_send, NULL, (THREADFUNCPTR) &TCPSender::senderThread, this))
            qDebug()<<"now we should quit"; //quit("\n--> pthread_create failed.", 1);
    }


    // use add_job to hand over send requests, non blocking, set MODE if you await a reply
    void add_job (char* t_data_ptr, uint32_t* t_data_length_ptr, uint8_t mode)
    {

        this->job_queue.insert( job_queue.begin(),  { t_data_ptr, t_data_length_ptr, mode });
    }

    void close_connection (void)
    {
        qDebug()<<"closing Socket";
        close(clientSock);
        pthread_mutex_lock(&thread_state_mutex);
        thread_state = STATE_CONNECTION_CLOSED;
        pthread_mutex_unlock(&thread_state_mutex);

    }



private:
    typedef void * (*THREADFUNCPTR)(void *);

    void* senderThread(void* arg)
    {
        struct  sockaddr_in serverAddr;
        socklen_t           serverAddrLen = sizeof(serverAddr);

        /* make this thread cancellable using pthread_cancel() */
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

        if ((clientSock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
           qCritical()<<"socket() failed";
        }
        serverAddr.sin_family = PF_INET;

        //inet_aton() converts the Internet host address cp from the IPv4 numbers-and-dots notation into binary form (in network byte order) and stores it in the structure that inp points to.
        if (inet_aton(SERVERIP, &serverAddr.sin_addr) == 0)
            qCritical()<<"Inet Aton Failed";

        //serverAddr.sin_addr.s_addr = inet_addr(server_ip);

        serverAddr.sin_port = htons(SERVERPORT);

        if (::connect(clientSock, (sockaddr*) &serverAddr, serverAddrLen) < 0) {
            qCritical()<<"connect() failed";
        }

        int bytes_sent;
        static job_type t_job;
        static char rx_buffer[512];

        /* start sending thread */
        while(1)
        {
            pthread_mutex_lock(&thread_state_mutex);
            switch (thread_state)
            {
            case STATE_WAITING_FOR_JOBS:
                if (job_queue.size())
                {
                    qDebug()<<"SIZE JOB Q :"<< job_queue.size();
                    pthread_mutex_lock(&job_list_mutex);

                    t_job = job_queue.back();
                    job_queue.pop_back();

                    if ((bytes_sent = send(clientSock, t_job.data_ptr, *t_job.data_length_ptr, 0)) < 0){
                        qCritical()<< "send() failed! " << bytes_sent;
                    }
                    pthread_mutex_unlock(&job_list_mutex);

                    /* if something went wrong, restart the connection */
                    if (bytes_sent != *t_job.data_length_ptr)
                    {
                        qCritical()<< "sent bytes cnt != bytes to send cnt -->  Connection Terminated!";
                        close(clientSock);
                        if (::connect(clientSock, (sockaddr*) &serverAddr, serverAddrLen) == -1) {
                            qCritical()<< "Reconnect Failed!";
                        }
                    }
                }


                if (t_job.mode == SEND_MODE_WITH_REPLY)
                {
                    thread_state = STATE_WAITING_FOR_REPLY;
                    startMDelayTimer();
                    printf("timer start");
                }

                break;
            case STATE_WAITING_FOR_REPLY:
                // only for paranoidz.
                // memset(&buffer[0], 0, sizeof(buffer));
                int bytes_received = 0;
                if( recv(clientSock , rx_buffer ,  bytes_received, 0) < 0)
                {
                    printf("a");
                    qCritical()<<"Receive Failed";
                }
                if (bytes_received > 0)
                {
                    stopMDelayTimer();
                    printf("b");
                    qDebug()<<"Data Received"<<rx_buffer;
                    // TODO
                }
                break;
            }
            pthread_mutex_unlock(&thread_state_mutex);
            /* have we terminated yet? */
            pthread_testcancel();
            /* no, take a rest for a while */
            usleep(100);   //1000 Micro Sec
        }
    }

};

#endif // TCPSENDER_H
