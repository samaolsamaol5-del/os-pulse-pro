#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #define SLEEP_SEC(s) Sleep((s) * 1000)
    #define SLEEP_MS(ms) Sleep(ms)
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <signal.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #define SLEEP_SEC(s) sleep(s)
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#else
    #include <unistd.h>
    #include <signal.h>
    #define SLEEP_SEC(s) sleep(s)
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

// 1. دالة قراءة استهلاك المعالج الحقيقي لكل نظام
double get_real_cpu_usage() {
#ifdef _WIN32
    FILETIME idle1, kernel1, user1, idle2, kernel2, user2;
    GetSystemTimes(&idle1, &kernel1, &user1);
    SLEEP_MS(500);
    GetSystemTimes(&idle2, &kernel2, &user2);

    ULARGE_INTEGER i1 = {{idle1.dwLowDateTime, idle1.dwHighDateTime}};
    ULARGE_INTEGER k1 = {{kernel1.dwLowDateTime, kernel1.dwHighDateTime}};
    ULARGE_INTEGER u1 = {{user1.dwLowDateTime, user1.dwHighDateTime}};
    ULARGE_INTEGER i2 = {{idle2.dwLowDateTime, idle2.dwHighDateTime}};
    ULARGE_INTEGER k2 = {{kernel2.dwLowDateTime, kernel2.dwHighDateTime}};
    ULARGE_INTEGER u2 = {{user2.dwLowDateTime, user2.dwHighDateTime}};

    ULONGLONG idle_d = i2.QuadPart - i1.QuadPart;
    ULONGLONG total_d = (k2.QuadPart - k1.QuadPart) + (u2.QuadPart - u1.QuadPart);
    if (total_d == 0) return 0.0;
    return (double)(total_d - idle_d) / total_d * 100.0;

#elif defined(__APPLE__)
    host_cpu_load_info_data_t cpu1, cpu2;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu1, &count);
    SLEEP_MS(500);
    host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu2, &count);

    unsigned long user_d = cpu2.cpu_ticks[CPU_STATE_USER] - cpu1.cpu_ticks[CPU_STATE_USER];
    unsigned long sys_d  = cpu2.cpu_ticks[CPU_STATE_SYSTEM] - cpu1.cpu_ticks[CPU_STATE_SYSTEM];
    unsigned long nice_d = cpu2.cpu_ticks[CPU_STATE_NICE] - cpu1.cpu_ticks[CPU_STATE_NICE];
    unsigned long idle_d = cpu2.cpu_ticks[CPU_STATE_IDLE] - cpu1.cpu_ticks[CPU_STATE_IDLE];
    unsigned long total_d = user_d + sys_d + nice_d + idle_d;
    if (total_d == 0) return 0.0;
    return (double)(total_d - idle_d) / total_d * 100.0;

#else
    long u1, n1, s1, i1, io1, irq1, sirq1, st1;
    long u2, n2, s2, i2, io2, irq2, sirq2, st2;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &u1, &n1, &s1, &i1, &io1, &irq1, &sirq1, &st1);
    fclose(fp);

    SLEEP_MS(500);

    fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &u2, &n2, &s2, &i2, &io2, &irq2, &sirq2, &st2);
    fclose(fp);

    long prev_idle = i1 + io1, curr_idle = i2 + io2;
    long prev_total = prev_idle + u1 + n1 + s1 + irq1 + sirq1 + st1;
    long curr_total = curr_idle + u2 + n2 + s2 + irq2 + sirq2 + st2;
    long total_d = curr_total - prev_total, idle_d = curr_idle - prev_idle;
    if (total_d == 0) return 0.0;
    return (double)(total_d - idle_d) / total_d * 100.0;
#endif
}

// 2. دالة قراءة الرام الحقيقية لكل نظام
void get_real_ram_usage(long *total_mb, long *used_mb, double *ram_pct) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        *total_mb = (long)(status.ullTotalPhys / (1024 * 1024));
        long avail_mb = (long)(status.ullAvailPhys / (1024 * 1024));
        *used_mb = *total_mb - avail_mb;
        *ram_pct = (double)status.dwMemoryLoad;
    }
#elif defined(__APPLE__)
    int64_t mem_bytes = 0;
    size_t len = sizeof(mem_bytes);
    sysctlbyname("hw.memsize", &mem_bytes, &len, NULL, 0);
    *total_mb = (long)(mem_bytes / (1024 * 1024));

    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
        vm_size_t page_size;
        host_page_size(mach_host_self(), &page_size);
        long active = (long)((vm_stat.active_count + vm_stat.wire_count) * page_size / (1024 * 1024));
        *used_mb = active;
        *ram_pct = (*total_mb > 0) ? ((double)*used_mb / *total_mb) * 100.0 : 0.0;
    }
#else
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return;
    char line[256];
    long total_kb = 0, avail_kb = 0;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "MemTotal: %ld kB", &total_kb) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &avail_kb) == 1) continue;
    }
    fclose(file);
    *total_mb = total_kb / 1024;
    *used_mb = (total_kb - avail_kb) / 1024;
    *ram_pct = (total_kb > 0) ? ((double)(total_kb - avail_kb) / total_kb) * 100.0 : 0.0;
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

void kill_process_interactive() {
    printf("\n--- Interactive Process Manager ---\n");
    printf("Enter PID to terminate (or 0 to skip): ");
    int target_pid = 0;
    if (scanf("%d", &target_pid) == 1 && target_pid > 0) {
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)target_pid);
        if (hProcess != NULL) {
            if (TerminateProcess(hProcess, 1)) {
                printf("[SUCCESS] Process %d terminated successfully.\n", target_pid);
            } else {
                printf("[ERROR] Failed to terminate process %d.\n", target_pid);
            }
            CloseHandle(hProcess);
        } else {
            printf("[ERROR] Access denied or PID %d not found.\n", target_pid);
        }
#else
        if (kill(target_pid, SIGTERM) == 0) {
            printf("[SUCCESS] Process %d terminated successfully.\n", target_pid);
        } else {
            perror("[ERROR] Failed to terminate process");
        }
#endif
        SLEEP_SEC(2);
    }
}

void monitor_system(void) {
    double cpu_usage = get_real_cpu_usage();
    long total_mb = 0, used_mb = 0;
    double ram_usage = 0.0;
    get_real_ram_usage(&total_mb, &used_mb, &ram_usage);

    write_to_log(cpu_usage, ram_usage);

    printf("\033[H\033[J");
    printf("====================================================\n");
    printf("       [ OS-Pulse Pro v2.0 - Enterprise ]           \n");
    printf("====================================================\n");
    printf("CPU Usage   : %.2f%%\n", cpu_usage);
    if (cpu_usage > 80.0) printf(" [WARNING] High CPU Load Detected!\n");

    printf("Total RAM   : %ld MB\n", total_mb);
    printf("Used RAM    : %ld MB (%.2f%%)\n", used_mb, ram_usage);
    if (ram_usage > 80.0) printf(" [WARNING] High Memory Usage Detected!\n");

    printf("----------------------------------------------------\n");
    printf("Top Active Processes:\n");
#ifdef _WIN32
    system("tasklist /NH | sort /R /+65 | more +1");
#elif defined(__APPLE__)
    system("ps -eo pid,pcpu,pmem,comm -r | head -n 6");
#else
    system("ps -eo pid,%cpu,%mem,comm --sort=-%cpu | head -n 6");
#endif
    printf("====================================================\n");
}

int main(void) {
    for (int i = 0; i < 5; i++) {
        monitor_system();
        SLEEP_SEC(2);
    }
    kill_process_interactive();
    return 0;
}

