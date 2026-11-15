#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

typedef struct {
    long user, nice, system, idle, iowait, irq, softirq, steal;
} CpuTimes;

CpuTimes get_cpu_times() {
    FILE *fp = fopen("/proc/stat", "r");
    CpuTimes times = {0};
    if (!fp) return times;

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        sscanf(buffer, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
               &times.user, &times.nice, &times.system, &times.idle,
               &times.iowait, &times.irq, &times.softirq, &times.steal);
    }
    fclose(fp);
    return times;
}

double calculate_cpu_usage(CpuTimes prev, CpuTimes curr) {
    long prev_idle = prev.idle + prev.iowait;
    long curr_idle = curr.idle + curr.iowait;

    long prev_non_idle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
    long curr_non_idle = curr.user + curr.nice + curr.system + curr.irq + curr.softirq + curr.steal;

    long prev_total = prev_idle + prev_non_idle;
    long curr_total = curr_idle + curr_non_idle;

    long total_d = curr_total - prev_total;
    long idle_d = curr_idle - prev_idle;

    if (total_d == 0) return 0.0;
    return (double)(total_d - idle_d) / total_d * 100.0;
}

void send_cloud_alert(const char *message) {
    FILE *alert_log = fopen("cloud_alerts.log", "a");
    if (alert_log) {
        time_t now = time(NULL);
        fprintf(alert_log, "[ALERT @ %s] %s\n", ctime(&now), message);
        fclose(alert_log);
    }
}

void monitor_system(void) {
    CpuTimes cpu1 = get_cpu_times();
    usleep(500000); 
    CpuTimes cpu2 = get_cpu_times();
    double cpu_usage = calculate_cpu_usage(cpu1, cpu2);

    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return;

    char line[256];
    long total_memory = 0, free_memory = 0, available_memory = 0;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "MemTotal: %ld kB", &total_memory) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &free_memory) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &available_memory) == 1) continue;
    }
    fclose(file);

    long used_memory = total_memory - available_memory;
    double ram_usage = ((double)used_memory / total_memory) * 100.0;

    // مسح الشاشة وطباعة الشعار الجذاب بالألوان
    printf("\033[H\033[J");
    printf("\033[1;36m"); // لون سماوي جذاب للشعار
    printf("  ____  _____   ____  _   _ _     ____  _____ \n");
    printf(" / __ \\/ ____| |  _ \\| | | | |   / ___|| ____|\n");
    printf("| |  | \\__ \\   | |_) | | | | |   \\___ \\|  _|  \n");
    printf("| |__| |___) |  |  __/| |_| | |___ ___) | |___ \n");
    printf(" \\____/_____/   |_|    \\___/|_____|____/|_____|\n");
    printf("\033[0m"); // إعادة اللون للوضع الطبيعي
    printf("==================================================\n");
    printf("       [ Enterprise Cloud Monitor v3.1 ]          \n");
    printf("==================================================\n");
    
    printf("CPU Usage   : %.2f%%\n", cpu_usage);
    if (cpu_usage > 70.0) {
        printf(" \033[1;31m[CLOUD ALERT] High CPU Load Detected!\033[0m\n");
        send_cloud_alert("WARNING: High CPU Load detected on Server!");
    }

    printf("Total RAM   : %ld MB\n", total_memory / 1024);
    printf("Used RAM    : %ld MB (%.2f%%)\n", used_memory / 1024, ram_usage);
    if (ram_usage > 70.0) {
        printf(" \033[1;31m[CLOUD ALERT] High Memory Usage Detected!\033[0m\n");
        send_cloud_alert("WARNING: High Memory Usage detected on Server!");
    }

    printf("--------------------------------------------------\n");
    printf("Top Active Processes:\n");
    system("ps -eo pid,%cpu,%mem,comm --sort=-%cpu | head -n 5");
    printf("==================================================\n");
    printf("Press Ctrl+C to exit. (Cloud sync active ⚡)\n");
}

int main(void) {
    while (1) {
        monitor_system();
        sleep(2);
    }
    return 0;
}

