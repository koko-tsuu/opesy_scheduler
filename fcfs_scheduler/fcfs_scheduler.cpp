#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <vector>
#include <fstream>
#include <thread>
#include <shared_mutex> 
#include <mutex>
#include <chrono>
#include <memory>
#include <condition_variable>
#include <random>

#include <windows.h> // Sleep to not automatically complete the tasks

// NOTE: all to-dos are related to MO1

/*
*  Listing down to-dos because i'm getting confused lol
*   initialize command -> integrate config to code
*           scheduler + quantum-cycles-> integrate roundrobin algorithm 
*   screen -s -> process-smi + user should not be able to access the process after its finished execution
*   report-util -> save this into a text file "csopesy-log.txt"
*   screen -ls -> we have this already, maybe clean up lang with the printing (no running processes chuchu)
* 
*  TO CHANGE: cpu cycle related stuff, i don't know if it's good enough?
*/

using namespace std;
class Core;
class Screen;
class Console;
class Schedule;

void clearScreen(bool print_header = true);
string getCurrentTimestamp();
void printHeader();



class Screen {
public:
    string process_name;

    enum ProcessStatus {
        READY,
        RUNNING,
        WAITING,
        FINISHED
    };

    ProcessStatus status = READY;

    int pid;
    int curr_line_instr;
    int total_line_instr;
    int core_id_assigned = -1;

    //--------------- unused part
    enum Command {
        PRINT
    };

    vector<Command> commands;

    //---------------


    string created_at;

    Screen(string process_name,
        int pid,
        int curr_line_instr,
        int total_line_instr,
        string created_at)
        :
        process_name(process_name),
        pid(pid),
        curr_line_instr(0),
        total_line_instr(total_line_instr),
        created_at(created_at) {}

    Screen(string process_name,
        ProcessStatus status,
        int pid,
        int total_line_instr,
        vector<Command> commands,
        string created_at)
        :
        process_name(process_name),
        status(status),
        pid(pid),
        curr_line_instr(0),
        total_line_instr(total_line_instr),
        created_at(created_at) {}

    void printScreen() {
        std::cout << "Process Name: " << process_name << std::endl;
        std::cout << "PID: " << pid << std::endl;
        std::cout << std::endl;
        std::cout << "Current Instruction Line: " << curr_line_instr << std::endl;
        std::cout << "Lines of Code: " << total_line_instr << std::endl;
        std::cout << "Created At: " << created_at << std::endl;

        if (status == FINISHED)
            std::cout << std::endl << "Finished!" << std::endl;
    }

    void executeCommand() {

        status = RUNNING;

        curr_line_instr++;

        if (curr_line_instr == total_line_instr)
            status = FINISHED;
       
          
    }




};


class Core {
public:
    int id;
    int delayConfig;
    int delay;
    shared_ptr<Screen> process_to_execute = nullptr;

    std::mutex mtx;
    

    Core(int id, int delay) : id(id), delay(delay) {}

    void run_core() {
        std::unique_lock<std::mutex> lock(mtx);
        if (process_to_execute != nullptr)
        {
            if (delay == 0) // instruction can be executed
            {
                delay = delayConfig;

                process_to_execute->executeCommand();

            }
            else  // busy
                delay--;
   
        }

    }
};

class Schedule {
public:
    vector<shared_ptr<Core>> coresAvailable;
    vector<shared_ptr<Screen>> readyQueue;
    int quantumCycle = -1;
    
    enum SchedulingAlgorithm {
        FCFS,
        RR
    };

    SchedulingAlgorithm schedulingAlgo;


    Schedule() {}

    int initialize_scheduler(string algoSelected) // to check if input was valid
    {
        algoSelected = algoSelected.substr(1, algoSelected.length() - 2);

        if (algoSelected == "fcfs")
        {
            schedulingAlgo = this->FCFS;
            return 0;
        }
        else if (algoSelected == "rr")
        {
            schedulingAlgo = this->RR;
            return 0;
        }
        else
        {
            // error
            return 1;
        }
    }

    void run_scheduler()
    {
        if (schedulingAlgo == this->FCFS)
            run_fcfs();
        
        else if (schedulingAlgo == this->RR)
            run_rr();
        
    }


    // this is to be ran sequentially from main loop
    void run_fcfs() {
        if (!readyQueue.empty()) {
            for (auto& core : coresAvailable) {
                // Check if the core is free
                if (core->process_to_execute == nullptr) {
                    core->process_to_execute = readyQueue.front();
                    readyQueue.erase(readyQueue.begin());

                    // Update the process's core ID
                    core->process_to_execute->core_id_assigned = core->id;
                }
            }


        }

        
        
        
    }

    void run_rr() {
        int currentQuantum = quantumCycle;
        
            // TO-DO, round robin algo
          
        
    }





    // debug display for ready queue and core status
    void debugSchedulerState() {
        std::cout << "==== Scheduler State ====" << std::endl;

        std::cout << "Ready Queue:" << std::endl;
        for (const auto& screen : readyQueue) {
            std::cout << " - " << screen->process_name << " (PID: " << screen->pid << ")" << std::endl;
        }

        std::cout << "Core States:" << std::endl;
        for (const auto& core : coresAvailable) {
            std::cout << "Core " << core->id << ": ";
            if (core->process_to_execute) {
                std::cout << "Running process " << core->process_to_execute->process_name;
            }
            else {
                std::cout << "Free";
            }
            std::cout << std::endl;
        }

        std::cout << "=========================" << std::endl;
    }
    



};


class Console {
private:
    Schedule scheduler;

    int freqProcess = -1;
    int freq;
    int minCommand = -1;
    int maxCommand = -1;
    int delayExec = -1;
    int currentProcess = 0;

    bool hasInitialized = false;
    bool toStartCreatingProcess = false;
    vector<std::shared_ptr<Screen>> screens;
    bool isMainMenu;
    int cpuCycles = 0;

    std::vector<std::thread> listOfCoreThreads;



public:
    Console() : isMainMenu(true){}

    void start() {

        clearScreen(true);
        string command, option, process_name;

        while (true) {
            std::cout << "Enter a command: ";
            std::getline(std::cin, command);

            if (command == "initialize") {
                if (hasInitialized)
                    std::cout << "CPU is already initialized." << std::endl;
                else
                    initialize();
            }
            else if (hasInitialized)
            {
                if (command == "exit") {
                    std::cout << "Exiting program... Goodbye!" << std::endl;
                    break;
                }
                
                else if (command.find("screen") == 0) {

                    std::istringstream iss(command.substr(6));
                    iss >> option >> process_name;

                    handleScreenCommand(option, process_name);
                }
                else if (command == "clear") {
                    clearScreen(true);
                }
                else if (command == "scheduler-test") {
                    toStartCreatingProcess = true;
                    std::cout << "scheduler-test activated." << std::endl;
                }
                else if (command == "scheduler-stop") {
                    if (!toStartCreatingProcess)
                        std::cout << "No ongoing scheduler-test at the moment." << std::endl;
                    else {
                        toStartCreatingProcess = false;
                        std::cout << "Stopped scheduler-test." << std::endl;
                    }
                }
                else {
                    std::cout << "Unknown command. Please try again." << std::endl;
                }
            }
            else
            {
                std::cout << "Commands cannot be accepted. CPU needs to be initialized." << std::endl;
            }
        }
    }

    void scheduler_test()
    {
           
        // we made it -1 in instantiating batch-per-freq, so it's centered at 0
        if (freq == 0) // if statement will dictate if it will create a process
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(minCommand, maxCommand); // randomize the amount of commands
            std::shared_ptr<Screen> screen = std::make_shared<Screen>(Screen("process" + std::to_string(currentProcess), currentProcess, 0, dist(gen), getCurrentTimestamp()));
            screens.push_back(screen);
            scheduler.readyQueue.push_back(screens.back());

            currentProcess++;
            freq = freqProcess;
        }
        else {
            freq--;
        }
            
        
    }


    void simulateCpuCycle()
    {
        
        while (true) {  

            // 1. receive new processes
            if (toStartCreatingProcess)
                scheduler_test();

            // 2. run scheduler
            scheduler.run_scheduler();


            // 3. execute all cores
            for (int i = 0; i < listOfCoreThreads.size(); i++)
                listOfCoreThreads[i] = std::thread(&Core::run_core, scheduler.coresAvailable[i]);
       
            // wait until cores have finished execution
            for (int i = 0; i < listOfCoreThreads.size(); i++)
                listOfCoreThreads[i].join();
            
            cpuCycles++;

           
        }
    }




    void initialize() {
            ifstream readConfigFile("config.txt");
            
            if (!readConfigFile.is_open()) {
                std::cout << "Failed to read the config.txt file." << std::endl;
            }
            else {
                string cpuOption;
                string configInput;
                std::istringstream iss;
    
            try {
                // num-cpu
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear(); // clear stream
                    
                if (cpuOption != "num-cpu")
                    throw;

                int nCpuToInitialize = stoi(configInput);
                // to initialize cores after delay-per-exec is received
                

                // ------------------------ 

                // scheduler
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "scheduler")
                    throw std::exception("No option for scheduler");
                
                if (scheduler.initialize_scheduler(configInput)) // invalid config return 1
                    throw std::exception("Invalid scheduler option");


                // ------------------------ 

                // quantum-cycles
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "quantum-cycles")
                    throw std::exception("No option for quantum-cycles");

                scheduler.quantumCycle = stoi(configInput);

                // ------------------------ 

                // batch-process-freq
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "batch-process-freq")
                    throw std::exception("No option for batch-process-freq");

                freqProcess = stoi(configInput);

                // ------------------------
                // min-ins
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "min-ins")
                    throw std::exception("No option for min-ins");

                minCommand = stoi(configInput);

                // ------------------------

                // max-ins
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "max-ins")
                    throw std::exception("No option for max-ins");

                maxCommand = stoi(configInput);

                // ------------------------ 

                // delays-per-exec
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);               
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "delays-per-exec")
                    throw std::exception("No option for delays-per-exec");

                delayExec = stoi(configInput) + 1; // + 1 for easier time(?)

                initializeCores(nCpuToInitialize, delayExec);

                // ------------------------


                hasInitialized = true;

                std::cout << "Successfully read config.txt. Starting CPU cycles." << std::endl;

                // start cpu cycles
                std::thread(&Console::simulateCpuCycle, this).detach();

            }
            catch (std::exception& e) {
                std::cout << "Error in reading config.txt: " << e.what() << std::endl;
            } 
           }

           

    }


    void initializeCores(int numCores, int delay)
    {
        for (int i = 0; i < numCores; i++) {
            auto core = make_shared<Core>(i, delay);
            scheduler.coresAvailable.push_back(core);
            
        }

        listOfCoreThreads.resize(numCores); // reserve space in advance
    }


    void handleScreenCommand(const string& option, const string& process_name) {
        if (option == "-r" && process_name != "") {
            attachScreen(process_name);
        }
        else if (option == "-s" && process_name != "") {
            createScreen(process_name);
        }
        else if (option == "-ls") {
            listScreens();
        }
        // Lists core availability and ready queue along with the normal screen -ls
        else if (option == "-ls-debug") {
            listScreens(true);
        }
        else {
            std::cout << "Command not recognized." << std::endl;
        }
    }

    void createScreen(const string& process_name) {
        if (!checkExistingScreen(process_name)) {
            int pid = screens.size();
            std::shared_ptr<Screen> screen = make_shared<Screen>(Screen(process_name, pid, 0, 50, getCurrentTimestamp()));
            screens.push_back(screen);
            initScreen(screen);
        }
        else {
            std::cout << "Screen initialization failed. Please use another process name." << std::endl;
        }
    }

    // TO-DO: MO1: At any given time, any process can finish its execution. If this happens, the user can no longer access the screen after exiting.
    // to check if process has finished, then do not enter initScreen()
    void attachScreen(const string& process_name) {
        bool screenFound = false;
        for (int i = 0; i < screens.size(); i++) {
            if (screens[i]->process_name == process_name) {

                if (screens[i]->status == screens[i]->FINISHED)
                    break;
                else
                { 
                    initScreen(screens[i]);
                    screenFound = true;
                    break;
                }
            }
        }
        if (!screenFound) {
            std::cout << "Process " << process_name << " not found." << std::endl;
        }
    }

    void listScreens(bool debug = false) {
       
        if (screens.empty()) {
            std::cout << "No screens attached." << std::endl;
        }
        else {
            std::cout << "--------------------------------------" << std::endl;
            std::cout << "Running processes: " << std::endl;

            for (const auto& screen : screens) {
                if (screen->status == Screen::RUNNING)
                {
                    std::cout << screen->process_name << "    ";
                    std::cout << "(" + screen->created_at + ")    ";
                    std::cout << "Core: " + std::to_string(screen->core_id_assigned) << "    ";
                    std::cout << screen->curr_line_instr << " / " << screen->total_line_instr << "\n";
                  
                }
                
            }

            std::cout << std::endl;
            std::cout << "Finished processes: " << std::endl;

            for (const auto& screen : screens) {
                if (screen->status == Screen::FINISHED)
                {
                    std::cout << screen->process_name << "    ";
                    std::cout << "(" + screen->created_at + ")  ";
                    std::cout << "Finished     ";
                    std::cout << screen->curr_line_instr << " / " << screen->total_line_instr << "\n";
                }

                
            }
            std::cout << "--------------------------------------" << std::endl;

            // For calling screen -ls-debug
            if (debug) {
                scheduler.debugSchedulerState();
            }
        }
    }


    bool checkExistingScreen(const string& process_name) {
        for (const auto& screen : screens) {
            if (screen->process_name == process_name) {
                return true;
            }
        }
        return false;
    }

    void initScreen(std::shared_ptr<Screen>& screen) {
        clearScreen(false);
        screen->printScreen();

        while (true) {
            std::string command;
            std::cout << "root:/> ";
            std::getline(std::cin, command);

            if (command == "exit") {
                clearScreen(true);
                break;
            }
            else if (command == "clear") {
                clearScreen(false);
                screen->printScreen();
            }
            // 
            else if (command == "process-smi") {
                screen->printScreen();
            }
            else {
                std::cout << "Unknown command. Please try again." << std::endl;
            }
        }
    }
};

void clearScreen(bool print_header) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    if (print_header) {
        printHeader();
    }
}

void printHeader() {
    std::cout << R"(
  ____  ____   ___   ____   _____  ______    __
 / ___|/ ___| / _ \ |  _ \ | ____|/ ___\ \  / /
| |    \___ \| | | || |_) ||  _|  \___ \\ \/ / 
| |___  ___) | |_| ||  __/ | |___  ___) ||  |  
 \____|____/  \___/ |_|    |_____|____/  |_|
)" << std::endl;

    std::cout << "\033[32mHello, Welcome to CSOPESY commandline!\033[0m" << std::endl;
    std::cout << "\033[33mType 'exit' to quit, 'clear' to clear the screen\033[0m" << std::endl;
}

string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm local_time;
    localtime_s(&local_time, &now);

    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S%p", &local_time);

    return string(buffer);
}

int main() {

   Console console = Console();
   console.start();
    return 0;
}
