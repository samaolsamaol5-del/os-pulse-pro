#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <tlhelp32.h>
    #define SLEEP_MS(ms) Sleep(ms)
#elif __APPLE__
    #include <unistd.h>
    #include <signal.h>
    #include <mach/mach.h>
    #include <sys/sysctl.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#else
    #define _DEFAULT_SOURCE
    #include <unistd.h>
    #include <signal.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

// دالة حساب استهلاك المعالج حسب النظام
double get_cpu_usage() {
#ifdef _WIN32
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors = 0;
    static HANDLE self;

    if (numProcessors == 0) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;
        FILETIME ftime, fsys, fuser;
        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));
        self = GetCurrentProcess();
        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
        SLEEP_MS(500);
    }
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    double percent = (double)((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart));
    percent /= (now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;
    lastCPU = now; lastUserCPU = user; lastSysCPU = sys;
    return percent * 100.0;

#elif __APPLE__
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
        unsigned long total = 0;
        for (int i = 0; i < CPU_STATE_MAX; i++) total += cpuinfo.cpu_ticks[i];
        unsigned long idle = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
        return total > 0 ? (double)(total - idle) / total * 100.0 : 0.0;
    }
    return 0.0;

#else
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    long user, nice, sys, idle, iowait, irq, softirq, steal;
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
    fclose(fp);
    long total1 = user + nice + sys + idle + iowait + irq + softirq + steal;
    long idle1 = idle + iowait;

    SLEEP_MS(500);

    fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
    fclose(fp);
    long total2 = user + nice + sys + idle + iowait + irq + softirq + steal;
    long idle2 = idle + iowait;

    long total_d = total2 - total1;
    long idle_d = idle2 - idle1;
    return total_d == 0 ? 0.0 : (double)(total_d - idle_d) / total_d * 100.0;
#endif
}

// دالة حساب استهلاك الذاكرة حسب النظام
void get_ram_info(long *total_mb, long *used_mb, double *usage_pct) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    *total_mb = (long)(memInfo.ullTotalPhys / (1024 * 1024));
    long avail_mb = (long)(memInfo.ullAvailPhys / (1024 * 1024));
    *used_mb = *total_mb - avail_mb;
    *usage_pct = (double)memInfo.dwMemoryLoad;

#elif __APPLE__
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t physical_memory = 0;
    size_t length = sizeof(int64_t);
    sysctl(mib, 2, &physical_memory, &length, NULL, 0);
    *total_mb = (long)(physical_memory / (1024 * 1024));

    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
        long page_size = sysconf(_SC_PAGESIZE);
        *used_mb = (long)((vm_stats.active_count + vm_stats.wire_count) * page_size / (1024 * 1024));
    } else {
        *used_mb = 0;
    }
    *usage_pct = *total_mb > 0 ? ((double)(*used_mb) / (*total_mb)) * 100.0 : 0.0;

#else
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) { *total_mb = 0; *used_mb = 0; *usage_pct = 0.0; return; }
    char line[256];
    long total_kb = 0, avail_kb = 0;
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "MemTotal: %ld kB", &total_kb);
        sscanf(line, "MemAvailable: %ld kB", &avail_kb);
    }
    fclose(file);
    *total_mb = total_kb / 1024;
    *used_mb = (total_kb - avail_kb) / 1024;
    *usage_pct = total_kb > 0 ? ((double)(total_kb - avail_kb) / total_kb) * 100.0 : 0.0;
#endif
}

void write_to_log(double cpu, double ram) {
    FILE *log_file = fopen("os_pulse.log", "a");
    if (!log_file) return;
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strcspn(time_str, "\n")] = 0;
    fprintf(log_file, "[%s] CPU: %.2f%% | RAM: %.2f%%\n", time_str, cpu, ram);
    fclose(log_file);
}

// إنهاء العمليات بشكل متوافق مع جميع الأنظمة
void kill_process_interactive() {
    printf("\n--- Interactive Process Manager ---\n");
    printf("Enter PID to terminate (or 0 to skip): ");

    int target_pid = 0;
    if (scanf("%d", &target_pid) == 1 && target_pid > 0) {
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)target_pid);
        if (hProcess != NULL && TerminateProcess(hProcess, 1)) {
            printf("[SUCCESS] Process %d terminated successfully.\n", target_pid);
            CloseHandle(hProcess);
        } else {
            printf("[ERROR] Failed to terminate process %d (Error code: %lu)\n", target_pid, GetLastError());
            if (hProcess) CloseHandle(hProcess);
        }
#else
        if (kill(target_pid, SIGTERM) == 0) {
            printf("[\033[1;32mSUCCESS\033[0m] Process %d terminated successfully.\n", target_pid);
        } else {
            perror("[\033[1;31mERROR\033[0m] Failed to terminate process");
        }
#endif
        SLEEP_MS(2000);
    }
}

void monitor_system(void) {
    double cpu_usage = get_cpu_usage();
    long total_ram = 0, used_ram = 0;
    double ram_usage = 0.0;
    get_ram_info(&total_ram, &used_ram, &ram_usage);

    write_to_log(cpu_usage, ram_usage);

#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif

    printf("=========================================\n");
    printf("        [ OS-Pulse Pro v2.0 ]            \n");
    printf("=========================================\n");
    printf("CPU Usage   : %.2f%%\n", cpu_usage);
    if (cpu_usage > 80.0) printf(" [WARNING] High CPU Load Detected!\n");

    printf("Total RAM   : %ld MB\n", total_ram);
    printf("Used RAM    : %ld MB (%.2f%%)\n", used_ram, ram_usage);
    if (ram_usage > 80.0) printf(" [WARNING] High Memory Usage Detected!\n");

    printf("-----------------------------------------\n");
    printf("Top Active Processes:\n");
#ifdef _WIN32
    system("tasklist | findstr /V \"Image Name\" | more +1");
#elif __APPLE__
    system("ps -eo pid,%cpu,%mem,comm -r | head -n 6");
#else
    system("ps -eo pid,%cpu,%mem,comm --sort=-%cpu | head -n 6");
#endif
    printf("=========================================\n");
}

int main(void) {
    for (int i = 0; i < 3; i++) {
        monitor_system();
        SLEEP_MS(1500);
    }
    kill_process_interactive();
    return 0;
}

