#pragma once

#include "core/Types.h"
#include "process/PCB.h"

class ProcessManager {
public:
    ProcessManager();

    std::string create_pcb(const std::string& name, uint32_t required_time,
                           const std::string& owner, int queue_level = 0,
                           uint32_t parent_pid = 1);
    std::string kill_pcb(uint32_t pid);
    std::string block_pcb(uint32_t pid);
    std::string wakeup_pcb(uint32_t pid);
    std::string show_pcb(uint32_t pid) const;
    std::string list_pcb() const;
    std::string ptree() const;
    std::string suspend(uint32_t pid);
    std::string resume(uint32_t pid);
    std::string renice(uint32_t pid, int new_queue);

    PCB*       get_pcb(uint32_t pid);
    const PCB* get_pcb(uint32_t pid) const;
    uint32_t   get_next_pid() const { return next_pid_; }

    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);

private:
    std::vector<PCB> pcbs_;
    uint32_t         next_pid_ = 1;

    int find_index(uint32_t pid) const;
};
