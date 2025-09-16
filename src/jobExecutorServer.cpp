#include <netdb.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <queue>
#include <vector>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

class Job
{
private:
    int jobID;
    string job_ID;
    string job;
    int clientSocket;
    string triplet;

public:
    Job(int id, string job_ID, string jobname, int cliSock) : jobID(id), job_ID(job_ID + to_string(jobID)), job(jobname), clientSocket(cliSock) {}

    void set_triplet()
    {
        triplet = "<" + job_ID + "," + job + "," + to_string(clientSocket) + ">";
    }

    string get_triplet()
    {
        return triplet;
    }

    string get_job()
    {
        return job;
    }

    string get_job_ID()
    {
        return job_ID;
    }
};

// HERE IS THE STRUCT OF POINTERS FOR THE RECOURCES THAT ALL THE THREADS MUST HAVE ACCESS.
// SO INSTEAD OF GLOBALLY GENERATE THE RECOURCES WE ACESS THEM WITH POINTERS
struct ThreadArgs
{
    int *sock;
    queue<Job> *queueArg;
    int *Jcounter;
    int *BuffSizPointer;

    pthread_cond_t *cv;
    pthread_mutex_t *buffer_mutex;
    pthread_cond_t *cv_wrkr;
    pthread_mutex_t *concurr_mutex;

    int *concurrencyPtr;
    int *onRunCounter;
    int *IDsCounterPtr;
};

// FUNCTION FOR SEPERATE A COMMAND
vector<char *> arg_seperator(const string &command)
{
    // Tokenize the command into arguments using strtok
    std::vector<char *> argv;
    char *token = strtok(const_cast<char *>(command.c_str()), " ");
    while (token != nullptr)
    {
        argv.push_back(token);
        token = strtok(nullptr, " ");
    }
    argv.push_back(nullptr);

    cout << "from function argv is ";
    for (int i = 0; argv[i] != nullptr; i++)
    {
        cout << argv[i] << " ";
    }

    return argv;
}

// FUNCTION FOR CREATE THE SOCKET
int create_socket()
{
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        cerr << "Socket creation failed";
        return -1;
    }
    return server_fd;
}

// ATTACH THE SOCKET TO THE PORT
int attach_socket_to_port(int server_fd, int portnum)
{
    int opt = 1;
    struct sockaddr_in address;

    if ((setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0))
    {
        cerr << "Failed to attach socket to port";
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(portnum);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        cerr << "Bind failed";
        return -1;
    }
    return 0;
}
// LISTEN FROM SOCKET
int start_listening(int server_fd)
{
    if (listen(server_fd, 3) < 0)
    {
        cerr << "Listen failed";
        return -1;
    }
    return 0;
}

// A BUFFER PRINTING FUNCTION FOR DEBUGGING REASONS
void BufferPrinting(queue<Job> Q)
{

    if (Q.empty())
    {
        cout << "Empty" << endl;
    }
    queue<Job> tmp = Q;
    while (!tmp.empty())
    {
        Job job = tmp.front();
        cout << " " << job.get_triplet() << "|    ";
        tmp.pop();
    }

    cout << endl;
}

// THIS IS THE FUNCTION THAT WE CALL IN ORDER TO ACHIEVE THE "poll" COMMAND FUNCTIONALITY
string ReturnTheBuffer(queue<Job> Q)
{

    string message = "";
    if (Q.empty())
    {
        message = "Empty";
    }
    else
    {
        queue<Job> tmp = Q;
        while (!tmp.empty())
        {
            Job job = tmp.front();
            message += job.get_triplet() + " ";

            tmp.pop();
        }
    }

    return message;
}

// FOR "stop" COMMAND FUNCTIONALITY IMPLEMENTATION
bool RemoveFromBuffer(string input, void *arg)
{
    ThreadArgs *resource_args = static_cast<ThreadArgs *>(arg);
    queue<Job> *JobBuffer = resource_args->queueArg;
    pthread_cond_t *cv = resource_args->cv;
    pthread_mutex_t *buffer_mutex = resource_args->buffer_mutex;

    pthread_mutex_lock(buffer_mutex);

    queue<Job> tempQueue; // declare tempQueue as a regular queue<Job> object
    bool jobFound = false;

    while (!JobBuffer->empty())
    {
        Job &job = JobBuffer->front();
        if (job.get_job_ID() == input)
        {
            jobFound = true;
        }
        else
        {
            tempQueue.push(job); // call push() on the tempQueue object
        }
        JobBuffer->pop();
    }

    JobBuffer->swap(tempQueue); // swap the contents of JobBuffer and tempQueue

    pthread_mutex_unlock(buffer_mutex);

    return jobFound;
}

// THIS IS FUNCIONALITY OF THW WORKER THREAD: AS WE SEE WE PASS ALL THE POINTERS VIA THE STRUCK WE HAVE GENERATE
void *workerThread(void *arg)
{

    // cout<<"A worker Thread is running "<<endl;
    // cout<<"before segmentation 1"<<endl;
    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    // cout<<"before segmentation 2 "<<endl;
    int *socket = args->sock;
    // cout<<"before segmentation 3"<<endl;
    queue<Job> *bufferQueue = args->queueArg;
    int *counterPtr = args->Jcounter;
    int *bufferSizePtr = args->BuffSizPointer;
    int *concurrency = args->concurrencyPtr;
    int *OnRunJobs = args->onRunCounter;
    pthread_cond_t *cv = args->cv;
    pthread_mutex_t *buffer_mutex = args->buffer_mutex;
    pthread_cond_t *cv_wrkr = args->cv_wrkr;
    pthread_mutex_t *concurr_mutex = args->concurr_mutex;

    // AS WE SEE EACH TIME THAT WE WANT TO CHANGE A RESOURCE VALUE OR CHECK THIS VALUE, WE MUST LOCK THE RESPECTIVE MUTEX !!!!
    while (true)
    {

        if (bufferQueue->empty()) // If the buffer queue is empty, the worker thread will sleep until it is signaled by the controller thread that a job has been added to the queue

        {

            pthread_mutex_lock(buffer_mutex);
            cout << "I am worker, and I am gonna wating" << endl;
            pthread_cond_wait(cv, buffer_mutex);
            pthread_mutex_unlock(buffer_mutex);
        }

        // Here the worker thread is now awake
        cout << "I am a worker and I am awake now" << endl;
        // cout<<"FROM WORKER TRHEAD CONCURRENCY IS"<<(*concurrency)<<endl;
        // cout<<"FROM worker trhead RUNJOBS ARE "<<(*OnRunJobs)<<endl;
        pthread_mutex_lock(buffer_mutex);
        if ((*OnRunJobs) < (*concurrency))
        {

            pthread_cond_signal(cv_wrkr);

            pthread_mutex_unlock(buffer_mutex);
            string jobToEx = bufferQueue->front().get_job();
            vector<char *> argv = arg_seperator(jobToEx);

            pthread_mutex_lock(buffer_mutex);
            bufferQueue->pop();
            pthread_mutex_unlock(buffer_mutex);

            pthread_mutex_lock(buffer_mutex);
            (*OnRunJobs)++;
            pthread_mutex_unlock(buffer_mutex);
            pthread_mutex_lock(buffer_mutex);
            (*counterPtr)--;
            cout << "---------------------- jobs that runnin are " << (*OnRunJobs) << endl;
            pthread_mutex_unlock(buffer_mutex);

            /* HERE WE ARE AT THE EXECUTION */

            pid_t pid_c = fork();

            if (pid_c == 0)
            {

                /* !!!!!!!!!! HERE I ATEMPTED TO SEND THE OUPUT AT THE CLIENT BUT IT SEMMS TO NOT SEND THE OUPUT AT THE RIGHT CLIENT. I WAS OUT OF TIMING SO I COMMENTED THESE */

                // Step 1: Create a new file with the name "pid.output"
                // string filename = to_string(getpid()) + ".output";
                // int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                // if (fd == -1) {
                //     cerr << "Error: Failed to create output file." << endl;
                //     exit(1);
                // }

                // // Step 2: Redirect STDOUT to the new file using dup2()
                // if (dup2(fd, STDOUT_FILENO) == -1) {
                //     cerr << "Error: Failed to redirect STDOUT." << endl;
                //     exit(1);
                // }
                execvp(argv[0], &argv[0]);
            }
            else if (pid_c < 0)
            {
                cout << "Failed to fork process" << endl;
            }
            else
            {
                cout << "Parent process" << endl;
                waitpid(pid_c, NULL, 0);

                /* open the OUTPUT file in order to read the content*/

                // string filename = to_string(pid_c) + ".output";
                // int fd = open(filename.c_str(), O_RDONLY);
                // if (fd == -1) {
                //     cerr << "Error: Failed to open output file." << endl;
                //     exit(1);
                // }

                // // Step 3: Read the contents of the file using read()
                // ssize_t file_size = lseek(fd, 0, SEEK_END);
                // lseek(fd, 0, SEEK_SET);
                // char* contents = new char[file_size + 1];
                // ssize_t bytes_read = read(fd, contents, file_size);
                // if (bytes_read == -1) {
                //     cerr << "Error: Failed to read output file." << endl;
                //     exit(1);
                // }
                // contents[bytes_read] = '\0';
                // close(fd);
                // string message = contents;
                // send to client the output
                // send((*socket),message.c_str(),message.length(),0);               // HERE IT WAS SENDING THE OUPUT AT THE CLIENT. BUT NOT THE RIGHT CLIENT. SO THIS IS THE REASON I'VE COMMENTED ALL THESE

                pthread_mutex_lock(buffer_mutex); // Lock the mutex
                (*OnRunJobs)--;                   // Decrement the counter
                // pthread_cond_signal(cv_wrkr);
                pthread_mutex_unlock(buffer_mutex); // Unlock the mutex
            }

            /* HERE IS THE END OF THE EXECUTION*/
        }
        else
        {
            pthread_mutex_unlock(buffer_mutex);
            cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!Concurrency does not allow me to run this job" << endl;
            pthread_mutex_lock(concurr_mutex);
            pthread_cond_wait(cv_wrkr, concurr_mutex); // HERE THIS WORKER TRHEAD WILL BE INFORMED BY THE CONTROLLER THAT CONCURRENCY HAS CHANGED

            pthread_mutex_unlock(concurr_mutex);
        }
    }

    pthread_exit(NULL);
}

void *controllerThread(void *arg)
{
    /*Controller Thread*/
    // cout<<"Hello from Controller Thread"<<endl;
    /*pass the arguments here*/
    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    int *socket = args->sock;
    queue<Job> *queue = args->queueArg;
    int *counterPtr = args->Jcounter;
    int *buffSizePtr = args->BuffSizPointer;
    int *concPtr = args->concurrencyPtr;
    int *IDPtr = args->IDsCounterPtr;
    // cout<<endl<<"From controller thread bufferSize is "<<(*buffSizePtr)<<endl;
    pthread_cond_t *cv = args->cv;
    pthread_mutex_t *buffer_mutex = args->buffer_mutex;
    pthread_cond_t *cv_wrkr = args->cv_wrkr;

    // cout<<"counter is"<<(*counterPtr)<<endl;

    // cout<<"counter is"<<(*counterPtr)<<endl;
    // cout<<"Controller thread is running"<<endl;

    char read_message[1024] = {0};
    // Receive data from the client
    read((*socket), read_message, 1024);
    cout << "From controller thread Message received: " << read_message << endl;

    // Tokenize the message into words
    char *token = strtok(read_message, " ");
    // Store the first word and the rest of the message into separate variables
    string commandType = token;

    // Print the first word and the rest of the message for debugging purposes
    // cout << "First word: " << commandType << endl;

    // send(socket, "Hello from the server!", strlen("Hello from the server!"), 0);

    // cout<<"!!!!!!!!!!!! BEFORE IFS"<<endl;
    if (commandType == "issueJob")
    {

        string linuxCommand;
        while ((token = strtok(nullptr, " ")) != nullptr)
        {
            linuxCommand += token;
            linuxCommand += " ";
        }

        // Remove the trailing space from the rest of the message
        if (!linuxCommand.empty())
        {
            linuxCommand.pop_back();
        }

        pthread_mutex_lock(buffer_mutex); // LOCK THE RESPECTIVE MUTEX CAUSE WE NEED TO PLAY WITH VALUES OF THE RECOURCES THAT EVEREYONE HAS ACCESS!!

        if ((*counterPtr) < (*buffSizePtr)) // WE PUSH THE JOB ONLY IF BUFFERSIZE ALLOW IT !!!!!!!!!!!!
        {
            Job currentJob((*IDPtr), "job_", linuxCommand, (*socket));
            (*IDPtr)++;
            (*queue).push(currentJob);
            (*counterPtr)++;
            // cout<<"after pushing"<<endl;
            (*queue).back().set_triplet();

            string message = "JOB" + (*queue).back().get_triplet() + "SUBMITTED";
            // cout<<"message  "<<message.c_str()<<endl;

            send((*socket), message.c_str(), message.length(), 0);
            // SIGNAL THE CONDITION VARIABLE AFTER ADDING A NEW JOB TO BUFFER
            pthread_cond_signal(cv);
        }
        else
        {
            string message = "bufferSize does not allow to add other jobs";
            // cout<<"message  "<<message.c_str()<<endl;

            send((*socket), message.c_str(), message.length(), 0);
        }
        pthread_mutex_unlock(buffer_mutex);
    }
    else if (commandType == "setConcurrency")
    {
        string concurrencyInput;
        while ((token = strtok(nullptr, " ")) != nullptr)
        {
            concurrencyInput += token;
            concurrencyInput += " ";
        }
        pthread_mutex_lock(buffer_mutex);
        (*concPtr) = stoi(concurrencyInput);
        /* CONCURRENCY NOW IS DIFFERENT. WE MUST INFORM THE WORKER TRHEADS FOR THAT*/
        pthread_cond_signal(cv_wrkr);
        string message = "CONCURRENCY SET AT " + concurrencyInput;
        send((*socket), message.c_str(), message.length(), 0);
        pthread_mutex_unlock(buffer_mutex);
    }
    else if (commandType == "stop")
    {

        string input;
        while ((token = strtok(nullptr, " ")) != nullptr)
        {
            input += token;
            input += " ";
        }

        if (!input.empty())
        {
            input.pop_back();
        }

        bool flag = RemoveFromBuffer(input, args);
        if (flag)
        {
            string message = "JOB <" + input + ">" + " REMOVED";
            send((*socket), message.c_str(), message.length(), 0);
        }
        else
        {
            string message = "JOB <" + input + ">" + " NOT FOUND";
            send((*socket), message.c_str(), message.length(), 0);
        }
    }
    else if (commandType == "poll")
    {
        pthread_mutex_lock(buffer_mutex);
        string message = ReturnTheBuffer((*queue));
        send((*socket), message.c_str(), message.length(), 0);
        pthread_mutex_unlock(buffer_mutex);
    }
    else if (commandType == "exit")
    {

        send((*socket), "SERVER TERMINATED", strlen("SERVER TERMINATED"), 0);
        cout << "socket is " << socket << endl;
        pthread_exit(PTHREAD_CANCELED);
    }

    // pid_t pid = getpid();
    // cout<<"thread process id is "<<pid<<endl;

    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    /*Here we are at the Main Thread*/

    // default value of concurrency is 1
    int concurrency = 1;
    int OnRunningCounter = 0;
    // mutexes and condition vars
    //  here we initiate the mutexes and cond vars. after that we will pass them properly to the threads via pointers stuck
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    pthread_cond_t cv_wrkr = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t buffer_mutex PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t concurr_mutex PTHREAD_MUTEX_INITIALIZER;

    // declare the Buffer
    queue<Job> JobBuffer;
    int IDsCounter = 0;
    int JobCounter = 0;
    int new_socket = 0;
    /*INPUT ERROR HANDLING */
    if (argc < 3)
    {
        cerr << "Usage: ./jobExecutorServer [portnum] [bufferSize] [threadPoolSize]..." << endl;
        return -1;
    }
    // store the arguments here
    int portnum = atoi(argv[1]);
    int threadPoolSize = atoi(argv[3]);
    int bufferSize = atoi(argv[2]);
    ThreadArgs args = {&new_socket, &JobBuffer, &JobCounter, &bufferSize, &cv, &buffer_mutex, &cv_wrkr, &concurr_mutex, &concurrency, &OnRunningCounter, &IDsCounter};
    // Creation of Worker Threads
    //  pthread_t worker_trheadsIDs[threadPoolSize];
    for (int i = 0; i < threadPoolSize; i++)
    {
        pthread_t workerThreadID;
        int wrkr_status = pthread_create(&workerThreadID, NULL, workerThread, (void *)&args);
        if (wrkr_status)
        {
            cerr << "Error: Unable to create controller thread. Error code: " << wrkr_status << endl;
            exit(1);
        }
        // worker_trheadsIDs[i] = workerThreadID;
    }

    int server_fd = create_socket();
    cout << "server_fd IS " << server_fd << endl;
    if (server_fd == -1)
        return -1;

    if (attach_socket_to_port(server_fd, portnum) == -1)
        return -1;

    if (start_listening(server_fd) == -1)
        return -1;

    // loop in order to accept connections from jobCommanders
    while (true)
    {
        cout << "Concurrency is " << concurrency << endl;
        cout << "From main thread jobCounter is " << JobCounter << endl;
        cout << "From main thread JobBuffer is :";
        BufferPrinting(JobBuffer);

        // int new_socket;
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);

        // Accept an incoming connection
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        cout << "new_socket before if is " << new_socket << endl;
        if (new_socket < 0)
        {
            cerr << "Accept failed";
            // return -1;
        }
        else
        {

            pthread_t controllerThreadID;
            cout << "new_socket after if is " << new_socket << endl;

            int ctlr_status = pthread_create(&controllerThreadID, NULL, controllerThread, (void *)&args);
            if (ctlr_status)
            {
                cerr << "Error: Unable to create controller thread. Error code: " << ctlr_status << std::endl;
                exit(1);
            }

            // Wait for controller thread to complete its execution
            void *exit_ctlr_status;
            ctlr_status = pthread_join(controllerThreadID, &exit_ctlr_status);
            if (ctlr_status)
            {
                cerr << "Error: Unable to join controller trhead. Error code: " << ctlr_status << endl;
                exit(1);
            }

            ////////////////////////////////////////// I must find another specific parameter not this
            if (exit_ctlr_status == PTHREAD_CANCELED)
            {

                cout << "Controller thread was terminated by 'exit'. Code: " << ctlr_status << endl;

                exit(0);
            }
            else
            {
                cout << "Controller thread completed its execution with exit ctlr_status: " << exit_ctlr_status << endl;
            }
        }
    }

    return 0;
}
