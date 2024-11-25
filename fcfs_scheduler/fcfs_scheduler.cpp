#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <iostream>
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
#include <unordered_map>

#include <windows.h> // Sleep to not automatically complete the tasks


using namespace std;
class Core;
class Screen;
class Console;
class Schedule;
class Memory;

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

    string created_at;

    int frames_needed;

    int base_frame = -1; // for flat memory allocator
    int last_frame = -1; // for flat memory allocator

    int memory_to_occupy = -1;

    time_t placed_in_memory;
    vector<int> pages; 


    Screen(string process_name,
        int pid,
        int curr_line_instr,
        int total_line_instr,
        string created_at, int memory_to_occupy, int frames_needed)
        :
        process_name(process_name),
        pid(pid),
        curr_line_instr(0),
        total_line_instr(total_line_instr),
        created_at(created_at), 
        memory_to_occupy(memory_to_occupy),
        frames_needed(frames_needed) {
    }

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


class Memory {
public:
   
    std::unordered_map<int, boolean> flatMemoryAllocator; // used in memory allocator
    std::vector<int> availableFrames;                   // used in paging
    std::vector<std::shared_ptr<Screen>> processInMemory;
    int numberOfTotalFrames = -1;
    int maxMemory;
    int type;

    int numPagedIn = 0;
    int numPagedOut = 0;

    Memory() {};

    void initialize(int max, int memoryPerFrame, int type)
    {
        maxMemory = max;
        numberOfTotalFrames = maxMemory / memoryPerFrame;
        this->type = type;

        if (type == 0)
            for (int i = 0; i < numberOfTotalFrames; i++)
                flatMemoryAllocator.insert({ i, false });
        else
            for (int i = 0; i < numberOfTotalFrames; i++)
                availableFrames.push_back(i);
    }


    void deallocateFrames(std::shared_ptr<Screen> screen)
    {
        for (int i = 0; i < screen->frames_needed; i++)
            availableFrames.push_back(screen->pages[i]);
        screen->pages.clear();
        deleteProcessInMemory(screen);

    }

    void deleteProcessInMemory(std::shared_ptr<Screen> screen)
    {
        for (int i = 0; i < processInMemory.size(); i++)
        {
            if (processInMemory[i] == screen)
            {
                processInMemory.erase(processInMemory.begin() + i);
                break;
            }

        }

    }

    int returnFirstFitIndex(int framesNeeded)
    {
        int endRange = -1;
        int increment = 0;
        for (int i = 0; i < numberOfTotalFrames; i++)
        {
            if (availableFrames[i] == false)
            {
                endRange = i;
                increment++;
            }

            else {
                endRange = -1;
                increment = 0;
            }

            if (increment == framesNeeded && endRange != -1)
                return endRange - (framesNeeded - 1); // -1 to include the startIndex

        }

        return -1;
    }

    void allocate(std::shared_ptr<Screen> screen)
    {
        if (!checkIfProcessExistsInMemory(screen)) {
            if (type == 0)
                allocateFlatMemory(screen);
            else
                allocateFrames(screen);
        }

    }

    void storeBackingStore() // already contains deallocate
    {
        if (processInMemory.size() != 0) {
            int index = 0; // find oldest process
            // find a process to kick out
            for (int i = 0; i < processInMemory.size(); i++)
            {
                if (processInMemory[index]->placed_in_memory > processInMemory[i]->placed_in_memory) // index is more recent than i
                    index = i;

            }

            // store
            ofstream fileOPESY;
            fileOPESY.open(processInMemory[index]->process_name + ".txt");
            fileOPESY.close();

            // bring back frames
            deallocate(processInMemory[index]);

            numPagedOut++;
        }

        
    }

    boolean takeBackingStore(std::shared_ptr<Screen> screen)
    { 
        try {
            // take
            string fileToRemove = screen->process_name + ".txt";
            std::remove(fileToRemove.c_str());

            numPagedIn++;
            return true;
        }
        catch (exception e) {
            // 
            return false;
        }
    }

    void deallocate(std::shared_ptr<Screen> screen)
    {
        try {
            if (checkIfProcessExistsInMemory(screen)) {
                if (type == 0)
                    deallocateFlatMemory(screen);
                else
                    deallocateFrames(screen);

            }
            else
                throw;
        }
        catch (exception e) {
            std::cout << "Tried to deallocate a process that does not exist: " << e.what() << std::endl;
        }
    }


    void allocateFlatMemory(std::shared_ptr<Screen> screen)
    {
        takeBackingStore(screen);

        boolean isProcessAllocated = false;
        while (!isProcessAllocated)
        {

            int firstFitIndex = returnFirstFitIndex(screen->frames_needed);

            if (firstFitIndex != -1)
            {
                screen->base_frame = firstFitIndex;
                screen->last_frame = firstFitIndex + (screen->frames_needed - 1);

                for (int i = firstFitIndex; i <= firstFitIndex + (screen->frames_needed - 1); i++)
                    flatMemoryAllocator[i] = true;

                processInMemory.push_back(screen);
                screen->placed_in_memory = std::time(nullptr);
                isProcessAllocated = true;

            }
            else // find a process to kick out
                storeBackingStore();
                

        }

    }

    void deallocateFlatMemory(std::shared_ptr<Screen> screen)
    {
        for (int i = screen->base_frame; i <= screen->last_frame; i++)
            flatMemoryAllocator[i] = false;

        screen->base_frame = -1;
        screen->last_frame = -1;

        deleteProcessInMemory(screen);
    }


    void allocateFrames(std::shared_ptr<Screen> screen)
    {
        takeBackingStore(screen); // simulate

        boolean isProcessAllocated = false;
        while (!isProcessAllocated)
        {
            
            if (availableFrames.size() >= screen->frames_needed)
            {
                // take available frames
                for (int i = 0; i < screen->frames_needed; i++)
                    screen->pages.push_back(availableFrames[i]);

                availableFrames.erase(availableFrames.begin() + 0, availableFrames.begin() + screen->frames_needed); // recheck on this
                processInMemory.push_back(screen);
                screen->placed_in_memory = std::time(nullptr);
                isProcessAllocated = true;


            }
            else // kick a process out
                storeBackingStore();
         
            
        }

    }

    boolean checkIfProcessExistsInMemory(std::shared_ptr<Screen> screen)
    {
        for (int i = 0; i < processInMemory.size(); i++)
        {
            if (processInMemory[i] == screen)
                return true;
        }
        return false;
    }

    int getMemoryUsage()
    {
        int total = 0;
        for (int i = 0; i < processInMemory.size(); i++)
        {
            total += processInMemory[i]->memory_to_occupy;
        }

        return total;
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
    shared_ptr<Memory> memory;

    std::mutex mtx;

    Core(int id, int delayConfig, int quantumCycle, shared_ptr<Memory> memory) : id(id), delayConfig(delayConfig), quantumCycle(quantumCycle), memory(memory) {
        delay = delayConfig; 
        cycle = quantumCycle; 
        this->memory = memory;
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
                    memory->deallocate(process_to_execute);
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
    std::shared_ptr<Memory> memory;
    
    enum SchedulingAlgorithm {
        FCFS,
        RR
    };

    SchedulingAlgorithm schedulingAlgo;
    


    Schedule() { }

    int initialize_scheduler(string algoSelected, std::shared_ptr<Memory> memory) // to check if input was valid
    {
        algoSelected = algoSelected.substr(1, algoSelected.length() - 2);

        if (algoSelected == "fcfs")
        {
            schedulingAlgo = this->FCFS;
            initialize_memory(memory);
            return 0;
        }
        else if (algoSelected == "rr")
        {
            schedulingAlgo = this->RR;
            initialize_memory(memory);
            return 0;
        }
        else
        {
            // error
            return 1;
        }
    }

    void initialize_memory(std::shared_ptr<Memory> memory)
    {
        this->memory = memory;
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
                    memory->allocate(readyQueue.front());
                    
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
                        memory->allocate(readyQueue.front());
                        
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

                        // I assume process will stay in memory until another process needs to take its place
                        // memory->storeBackingStore(coresAvailable[i]->process_to_execute);

                        coresAvailable[i]->process_to_execute = nullptr;


                        // if another process can be executed in the ready queue
                        if (!readyQueue.empty())
                        {
                            memory->allocate(readyQueue.front());
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
    std::shared_ptr<Memory> memory;

    int freqProcess = -1;
    int freq;
    int minCommand = -1;
    int maxCommand = -1;
    int delayExec = -1;
    int currentProcess = 0;
    int memoryPerFrame;
    int minMemPerProc;
    int maxMemPerProc;

    bool hasInitialized = false;
    bool toStartCreatingProcess = false;
    vector<std::shared_ptr<Screen>> screens;
    bool isMainMenu;

    int cpuCycles = 0;
    int idleCycles = 0;
    int activeCycles = 0;

    std::vector<std::thread> listOfCoreThreads;
    std::thread cpuCycleThreadHolder;
    
    bool hasQuit = false;

    int quantumCyclePrint = 0; // temporary for this?


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
                    hasQuit = true; 
                    joinAllThreads();

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
                else if (command == "process-smi") {
                    int coresUsed = checkCoresUsed();
                    int memoryUsage = memory->getMemoryUsage();
                    std::cout << "-------------------------------------------------------------" << std::endl;
                    std::cout << "|                       PROCESS-SMI                         |" << std::endl;
                    std::cout << "-------------------------------------------------------------" << std::endl;
                    std::cout << "CPU-Util: " << std::round(((coresUsed * 1.0) / scheduler.coresAvailable.size()) * 100) << "%" << std::endl;
                    std::cout << "Memory Usage: " << memoryUsage << "KB / " << memory->maxMemory << "KB" << std::endl;
                    std::cout << "Memory Util: " << std::round(((memoryUsage * 1.0) / memory->maxMemory) * 100) << "%" << std::endl << std::endl;
                    std::cout << "==============================================================" << std::endl;
                    std::cout << "Running processes and memory usage:" << std::endl;
                    std::cout << "-------------------------------------------------------------" << std::endl;
                    
                    for (int i = 0; i < memory->processInMemory.size(); i++)
                        std::cout << memory->processInMemory[i]->process_name << " " << memory->processInMemory[i]->memory_to_occupy << "KB" << std::endl;
                    
                    std::cout << "-------------------------------------------------------------" << std::endl;


                        
                }
                else if (command == "vmstat") {
                    int memoryUsage = memory->getMemoryUsage();
                    int freeMemory = memory->maxMemory - memoryUsage;
                    std::cout << std::endl;
                    std::cout << memory->maxMemory << " total memory" << std::endl;
                    std::cout << memoryUsage << " used memory" << std::endl;
                    std::cout << freeMemory << " free memory" << std::endl;
                    std::cout << idleCycles << " idle CPU ticks" << std::endl;
                    std::cout << activeCycles << " active CPU ticks" << std::endl;
                    std::cout << cpuCycles << " total CPU ticks" << std::endl;
                    std::cout << memory->numPagedIn << " num paged in" << std::endl;
                    std::cout << memory->numPagedOut << " num paged out" << std::endl;
                    std::cout << std::endl;

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

    void joinAllThreads()
    {
        cpuCycleThreadHolder.join();
    }

    void scheduler_test()
    {

        // we made it -1 in instantiating batch-per-freq, so it's centered at 0
        if (freq == 0) // if statement will dictate if it will create a process
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(minCommand, maxCommand); // randomize the amount of commands
            std::uniform_int_distribution<> memory(minMemPerProc, maxMemPerProc);
            int generatedMemory = memory(gen);
            std::shared_ptr<Screen> screen = std::make_shared<Screen>(Screen("process" + std::to_string(currentProcess), currentProcess, 0, dist(gen), getCurrentTimestamp(), generatedMemory, ceil((generatedMemory * 1.0) / memoryPerFrame)));
            screens.push_back(screen);
            scheduler.readyQueue.push_back(screens.back());

            currentProcess++;
            freq = freqProcess;
            Sleep(200);
        }
        else {
            freq--;
        }
            
        
    }


    void simulateCpuCycle()
    {
        
        while (!hasQuit) {  

            // 1. receive new processes
            if (toStartCreatingProcess)
                scheduler_test();

            // 2. run scheduler
            scheduler.run_scheduler();

            // to see if this is an idle cycle, check if all cores have process
            if (isIdleCycle())
                idleCycles++;
            else
                activeCycles++;

            // 3. execute all cores
            for (int i = 0; i < listOfCoreThreads.size(); i++)
                listOfCoreThreads[i] = std::thread(&Core::run_core, scheduler.coresAvailable[i]);
       
            // wait until cores have finished execution
            for (int i = 0; i < listOfCoreThreads.size(); i++)
                listOfCoreThreads[i].join();


            
            cpuCycles++;

           
        }
    }


    boolean isIdleCycle()
    {
        for (int i = 0; i < listOfCoreThreads.size(); i++)
        {
            if (scheduler.coresAvailable[i]->process_to_execute != nullptr) // there is a process to be executed
                return false;
        }
        return true;
    }


    void initialize() {
            int quantumCycles;
            int maxMemory;
            
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
                    throw std::invalid_argument("No option for num-cpu");


                int nCpuToInitialize = stoi(configInput);
                // to initialize cores after delay-per-exec is received
                

                // ------------------------ 

                // scheduler
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "scheduler")
                    throw std::invalid_argument("No option for scheduler");

                string schedulerType = configInput;
                
               


                // ------------------------ 

                // quantum-cycles
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "quantum-cycles")
                    throw std::invalid_argument("No option for quantum-cycles");

                quantumCycles = stoi(configInput);

                // ------------------------ 

                // batch-process-freq
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "batch-process-freq")
                    throw std::invalid_argument("No option for batch-process-freq");

                freqProcess = stoi(configInput);

                // ------------------------
                // min-ins
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "min-ins")
                    throw std::invalid_argument("No option for min-ins");

                minCommand = stoi(configInput);

                // ------------------------

                // max-ins
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "max-ins")
                    throw std::invalid_argument("No option for max-ins");

                maxCommand = stoi(configInput);

                // ------------------------ 

                // delays-per-exec
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);               
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "delays-per-exec")
                    throw std::invalid_argument("No option for delays-per-exec");

                delayExec = stoi(configInput) + 1; // + 1 for easier time(?)

                // ------------------------

                // max-overall-mem
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "max-overall-mem")
                    throw std::invalid_argument("No option for max-overall-mem");
                
                maxMemory = stoi(configInput);
                

                // ------------------------

                // mem-per-frame
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "mem-per-frame")
                    throw std::invalid_argument("No option for mem-per-frame");

                memoryPerFrame = stoi(configInput);



                // ------------------------
                // 
                // mem-per-proc
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "min-mem-per-proc")
                    throw std::invalid_argument("No option for min-mem-per-proc");

                minMemPerProc = stoi(configInput);



                // ------------------------
                // 
                // mem-per-proc
                getline(readConfigFile, cpuOption);
                iss.str(cpuOption);
                iss >> cpuOption >> configInput;
                iss.clear();// clear stream

                if (cpuOption != "max-mem-per-proc")
                    throw std::invalid_argument("No option for max-mem-per-proc");

                maxMemPerProc = stoi(configInput);



                // ------------------------
                memory = make_shared<Memory>();
                initializeCores(nCpuToInitialize, delayExec, quantumCycles, memory);

                if (maxMemory == memoryPerFrame)
                    memory->initialize(maxMemory, memoryPerFrame, 0);
                else
                    memory->initialize(maxMemory, memoryPerFrame, 1);
      
                if (scheduler.initialize_scheduler(schedulerType, memory)) // invalid config return 1
                    throw std::invalid_argument("Invalid scheduler option");



                hasInitialized = true;

                std::cout << "Successfully read config.txt. Starting CPU cycles." << std::endl;

                // start cpu cycles
                cpuCycleThreadHolder = std::thread(&Console::simulateCpuCycle, this);

            }
            catch (std::exception& e) {
                std::cout << "Error in reading config.txt: " << e.what() << std::endl;
            } 
           }

           

    }


    void initializeCores(int numCores, int delay, int quantumCycles, std::shared_ptr<Memory> memory)
    {
        for (int i = 0; i < numCores; i++) {
            auto core = make_shared<Core>(i, delay, quantumCycles, memory);
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
            std::uniform_int_distribution<> mem(minMemPerProc, maxMemPerProc);
            int generatedMemory = mem(gen);

            int pid = screens.size();
            std::shared_ptr<Screen> screen = make_shared<Screen>(Screen(process_name, pid, 0, dist(gen), getCurrentTimestamp(), generatedMemory, ceil((generatedMemory * 1.0)/ memoryPerFrame)));
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
