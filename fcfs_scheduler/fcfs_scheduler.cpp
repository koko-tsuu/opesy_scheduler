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
        created_at(created_at) {
    }

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

    int executeCommand() {

        

        curr_line_instr++;

        if (curr_line_instr == total_line_instr)
            {
                status = FINISHED;
                return 0;
            }
        return 1;
       
          
    }




};


class Core {
public:
    int id;
    int delayConfig;
    int delay;
    int cycle;
    int quantumCycle = -1;
    shared_ptr<Screen> process_to_execute;

    std::mutex mtx;

    Core(int id, int delayConfig, int quantumCycle) : id(id), delayConfig(delayConfig), quantumCycle(quantumCycle) { 
        delay = delayConfig; 
        cycle = quantumCycle; 
    }

    void run_core() {
        std::unique_lock<std::mutex> lock(mtx);
        if (process_to_execute != nullptr)
        {
            if (delay == 0) // instruction can be executed
            {
                delay = delayConfig;

                if (process_to_execute->executeCommand() == 0) // 0 means all instructions have been executed
                {
                    process_to_execute = nullptr;
                    cycle = quantumCycle;
                }
                else
                    cycle--;
                
                

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
    
    enum SchedulingAlgorithm {
        FCFS,
        RR
    };

    SchedulingAlgorithm schedulingAlgo;


    Schedule() { }

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

        for (auto& core : coresAvailable) {
            if (!readyQueue.empty()) {
                // Check if the core is free
                if (core->process_to_execute == nullptr) {
                    core->process_to_execute = readyQueue.front();
                    readyQueue.erase(readyQueue.begin());

                    // Update the process's core ID
                    core->process_to_execute->core_id_assigned = core->id;
                    core->process_to_execute->status = Screen::RUNNING;
                   
                }
            }
            else
                break;

        }
        
        
    }

    void run_rr() {
       

        // this uses a different algorithm from fcfs
        for (int i = 0; i < coresAvailable.size(); i++) {

            // RR algorithm
      
                // Check if the core is free
                if (coresAvailable[i]->process_to_execute == nullptr) {
                    if (!readyQueue.empty())
                    {
                        coresAvailable[i]->process_to_execute = readyQueue.front();
                        readyQueue.erase(readyQueue.begin());

                        // Update the process's core ID
                        coresAvailable[i]->process_to_execute->core_id_assigned = coresAvailable[i]->id;
                        coresAvailable[i]->process_to_execute->status = Screen::RUNNING;
                    }
                  

                }
                else {

                    // check if we need to preempt
                    if (coresAvailable[i]->cycle == 0)
                    {
                        coresAvailable[i]->cycle = coresAvailable[i]->quantumCycle; // reset quantum cycle
                        readyQueue.push_back(coresAvailable[i]->process_to_execute); // put process back into ready queue
                        coresAvailable[i]->process_to_execute->status = Screen::READY;
                        coresAvailable[i]->process_to_execute = nullptr;

                        // if another process can be executed in the ready queue
                        if (!readyQueue.empty())
                        {
                            coresAvailable[i]->process_to_execute = readyQueue.front();
                            readyQueue.erase(readyQueue.begin());

                            // Update the process's core ID
                            coresAvailable[i]->process_to_execute->core_id_assigned = coresAvailable[i]->id;
                            coresAvailable[i]->process_to_execute->status = Screen::RUNNING;
                        }
                    }
                }
           

        }
          
        
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
                else if (command == "report-util")
                {

                    ofstream fileOPESY;
                    fileOPESY.open("csopesy-log.txt");
                    

                    // copied and pasted from screen -ls command
                    int coresUsed = checkCoresUsed();
                    fileOPESY << "CPU Utilization: " << std::round(((coresUsed * 1.0) / scheduler.coresAvailable.size()) * 100) << "%" << std::endl;
                    fileOPESY << "Cores used: " << coresUsed << std::endl;
                    fileOPESY << "Cores available: " << scheduler.coresAvailable.size() - coresUsed << std::endl << std::endl;

                    fileOPESY << "--------------------------------------" << std::endl;

                    if (screens.empty()) {
                        fileOPESY << "No screens attached." << std::endl;
                    }
                    else {

                        fileOPESY << "Running processes: " << std::endl;

                        for (const auto& screen : screens) {
                            if (screen->status == Screen::RUNNING)
                            {
                                fileOPESY << screen->process_name << "    ";
                                fileOPESY << "(" + screen->created_at + ")    ";
                                fileOPESY << "Core: " + std::to_string(screen->core_id_assigned) << "    ";
                                fileOPESY << screen->curr_line_instr << " / " << screen->total_line_instr << "\n";

                            }

                        }

                        fileOPESY << std::endl;
                        fileOPESY << "Finished processes: " << std::endl;

                        for (const auto& screen : screens) {
                            if (screen->status == Screen::FINISHED)
                            {
                                fileOPESY << screen->process_name << "    ";
                                fileOPESY << "(" + screen->created_at + ")  ";
                                fileOPESY << "Finished     ";
                                fileOPESY << screen->curr_line_instr << " / " << screen->total_line_instr << "\n";
                            }


                        }
                        fileOPESY << "--------------------------------------" << std::endl;

                    }
                    fileOPESY.close();
                        
                    //
                    std::cout << "Successfully printed report-util." << std::endl;
                }
                else if (command == "scheduler-test") {
                    if (toStartCreatingProcess)
                    {
                        std::cout << "scheduler-test is already activated." << std::endl;
                    }
                    else {
                        toStartCreatingProcess = true;
                        std::cout << "scheduler-test activated." << std::endl;
                    }
                    
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
         int quantumCycles;
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

                quantumCycles = stoi(configInput);

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

                initializeCores(nCpuToInitialize, delayExec, quantumCycles);

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


    void initializeCores(int numCores, int delay, int quantumCycles)
    {
        for (int i = 0; i < numCores; i++) {
            auto core = make_shared<Core>(i, delay, quantumCycles);
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
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(minCommand, maxCommand); // randomize the amount of commands

            int pid = screens.size();
            std::shared_ptr<Screen> screen = make_shared<Screen>(Screen(process_name, pid, 0, dist(gen), getCurrentTimestamp()));
            screens.push_back(screen);
            scheduler.readyQueue.push_back(screen);
            initScreen(screen);
        }
        else {
            std::cout << "Screen initialization failed. Please use another process name." << std::endl;
        }
    }

    // to check if process has finished, then do not enter initScreen()
    void attachScreen(const string& process_name) {
        bool screenFound = false;
        for (int i = 0; i < screens.size(); i++) {
            if (screens[i]->process_name == process_name) {

                if (screens[i]->status == Screen::FINISHED)
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

        int coresUsed = checkCoresUsed();
        std::cout << "CPU Utilization: " << std::round(((coresUsed * 1.0)/scheduler.coresAvailable.size()) * 100) << "%" << std::endl;
        std::cout << "Cores used: " << coresUsed << std::endl;
        std::cout << "Cores available: " << scheduler.coresAvailable.size() - coresUsed << std::endl << std::endl;

        std::cout << "--------------------------------------" << std::endl;
        
        if (screens.empty()) {
            std::cout << "No screens attached." << std::endl;
        }
        else {
            
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

    int checkCoresUsed()
    {
        int coresUsed = 0;
        for (int i = 0; i < scheduler.coresAvailable.size(); i++)
        {
            if (scheduler.coresAvailable[i]->process_to_execute != nullptr)
                coresUsed++;
        }

        return coresUsed;
        
    }


    bool checkExistingScreen(const string& process_name) {
        for (const auto& screen : screens) {
            if (screen->process_name == process_name) {
                return true;
            }
        }
        return false;
    }

    void initScreen(std::shared_ptr<Screen> screen) {
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
