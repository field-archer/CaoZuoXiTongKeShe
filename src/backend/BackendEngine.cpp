#include "backend/BackendEngine.h"
#include "core/Platform.h"
#include "core/Config.h"
#include <sstream>
#include <iomanip>
#include <functional>

BackendEngine::BackendEngine(const std::string& state_path)
    : state_path_(state_path) {
    load_state();
}

BackendEngine::~BackendEngine() {
    for (auto& [_, u] : users_)
        if (u.scheduler) { u.scheduler->stop(); u.scheduler->wait_for_stop(); }
}

UserState& BackendEngine::get_or_create_user(const std::string& username) {
    auto it = users_.find(username);
    if (it == users_.end()) {
        it = users_.emplace(username, UserState{}).first;
        it->second.init_scheduler(&process_mtx_);
    }
    return it->second;
}

void BackendEngine::load_state() {
    StateFile sf(state_path_);
    if (sf.exists()) sf.load(user_mgr_, users_);
    for (auto& [_, u] : users_) u.init_scheduler(&process_mtx_);
}

void BackendEngine::save_state() {
    StateFile sf(state_path_);
    sf.save(user_mgr_, users_);
}

static std::string help_text() {
    return R"(═══════════════ 命令列表 ═══════════════
【用户管理】
  register <用户名> <密码>     注册
  login <用户名> <密码>        登录
  logout                       登出

【进程管理】
  create_pcb <名称> <所需时间> [队列] [父PID]
  kill_pcb <PID>              撤销进程
  block_pcb <PID>             阻塞进程
  wakeup_pcb <PID>            唤醒进程
  show_pcb <PID>              查看进程详情
  list_pcb                    列出所有进程
  ptree                       进程树
  suspend <PID>               挂起进程
  resume <PID>                恢复挂起进程
  renice <PID> <Q>             移动到目标队列(0/1/2)

【调度器】
  start_sched                 启动自动调度
  stop_sched                  暂停调度
  restart_sched               重启调度
  step                        单步执行

【内存管理】
  alloc <大小> <PID>          分配内存(KB)
  free_mem <起始地址>         释放内存
  show_mem                    查看内存布局
  compact                     内存紧缩
  mem_stat                    内存统计
  set_alloc_algo <ff|bf|wf>   切换分配算法
  pgfault                     模拟缺页中断
  swap_out <PID>              进程内存换出

【持久化】
  save                        保存状态
  load                        加载状态

【可视化】
  overview                    系统全景视图
  help                        显示此帮助
  exit                        退出
══════════════════════════════════════════)";
}

static bool is_write_cmd(MessageType t) {
    switch (t) {
        case MessageType::REGISTER: case MessageType::CREATE_PCB:
        case MessageType::KILL_PCB: case MessageType::BLOCK_PCB:
        case MessageType::WAKEUP_PCB: case MessageType::SUSPEND:
        case MessageType::RESUME: case MessageType::RENICE:
        case MessageType::START_SCHED: case MessageType::STOP_SCHED:
        case MessageType::RESTART_SCHED: case MessageType::STEP:
        case MessageType::ALLOC: case MessageType::FREE_MEM:
        case MessageType::COMPACT: case MessageType::SET_ALLOC_ALGO:
        case MessageType::SWAP_OUT:
        case MessageType::SAVE: case MessageType::LOAD:
            return true;
        default: return false;
    }
}

std::string BackendEngine::dispatch(const Message& msg, const std::string& username) {
    // 登录守卫
    switch (msg.type) {
        case MessageType::REGISTER: case MessageType::LOGIN:
        case MessageType::LOGOUT: case MessageType::HELP:
        case MessageType::EXIT: break;
        default:
            if (username.empty())
                return "Error: 请先登录。使用 login <用户名> <密码>。";
    }

    auto& u = get_or_create_user(username);

    switch (msg.type) {
        case MessageType::REGISTER:
            return user_mgr_.register_user(arg(msg.args, 0), arg(msg.args, 1));
        case MessageType::LOGIN:
            return user_mgr_.login(arg(msg.args, 0), arg(msg.args, 1));
        case MessageType::LOGOUT:
            return "已登出。";

        // ═══ 进程 ═══
        case MessageType::CREATE_PCB: {
            uint32_t req = 0, parent = 1; int ql = 0;
            try {
                req = std::stoul(arg(msg.args, 1));
                if (msg.args.size() >= 3) ql = std::stoi(arg(msg.args, 2));
                if (msg.args.size() >= 4) parent = std::stoul(arg(msg.args, 3));
            } catch (...) { return "Error: create_pcb <名称> <所需时间> [队列] [父PID]"; }
            if (ql < 0 || ql > 2) return "Error: 队列为 0/1/2。";
            auto r = u.pm.create_pcb(arg(msg.args, 0), req, username, ql, parent);
            auto lp = r.find('('), rp = r.find(',', lp);
            if (lp != std::string::npos && rp != std::string::npos) {
                try {
                    uint32_t pid = std::stoul(r.substr(lp+1, rp-lp-1));
                    PCB* pcb = u.pm.get_pcb(pid);
                    if (pcb && !pcb->suspended) u.mlfq.enqueue_to_level(pid, ql);
                } catch (...) {}
            }
            return r;
        }
        case MessageType::KILL_PCB: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            std::vector<uint32_t> rm;
            PCB* t = u.pm.get_pcb(pid);
            if (t) {
                std::function<void(uint32_t)> collect = [&](uint32_t p) {
                    PCB* pc = u.pm.get_pcb(p); if (!pc) return;
                    for (auto c : pc->children_pids) collect(c);
                    rm.push_back(p);
                };
                for (auto c : t->children_pids) collect(c);
                rm.push_back(pid);
            }
            for (auto rp : rm) { u.mlfq.remove(rp); u.mm.free_all_by_pid(rp); }
            return u.pm.kill_pcb(pid);
        }
        case MessageType::BLOCK_PCB: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            u.mlfq.remove(pid);
            return u.pm.block_pcb(pid);
        }
        case MessageType::WAKEUP_PCB: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            PCB* pcb = u.pm.get_pcb(pid);
            if (!pcb) return "Error: PID 无效。";
            if (pcb->state == ProcessState::BLOCKED && pcb->swapped_out) {
                if (u.mm.pgfault_alloc(pid, pcb->swapped_size) != UINT32_MAX) {
                    pcb->swapped_out = false;
                    pcb->mem_block_addrs.clear(); pcb->mem_block_sizes.clear();
                    u.mm.get_pid_blocks(pid, pcb->mem_block_addrs, pcb->mem_block_sizes);
                } else {
                    return pcb->name + "(" + std::to_string(pid)
                           + ") 唤醒失败: 内存不足，仍阻塞。";
                }
            }
            auto r = u.pm.wakeup_pcb(pid);
            if (r.find("已唤醒") != std::string::npos && pcb && !pcb->suspended)
                u.mlfq.enqueue_to_level(pid, pcb->queue_level);
            return r;
        }
        case MessageType::SHOW_PCB: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            return u.pm.show_pcb(pid);
        }
        case MessageType::LIST_PCB: return u.pm.list_pcb();
        case MessageType::PTREE:    return u.pm.ptree();
        case MessageType::SUSPEND: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            u.mlfq.remove(pid);
            return u.pm.suspend(pid);
        }
        case MessageType::RESUME: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            auto r = u.pm.resume(pid);
            if (r.find("已恢复") != std::string::npos) {
                PCB* pcb = u.pm.get_pcb(pid);
                if (pcb) u.mlfq.enqueue_to_level(pid, pcb->queue_level);
            }
            return r;
        }
        case MessageType::RENICE: {
            uint32_t pid = 0; int q = 0;
            try { pid = std::stoul(arg(msg.args, 0)); q = std::stoi(arg(msg.args, 1)); }
            catch (...) { return "Error: renice <PID> <目标队列0/1/2>"; }
            u.mlfq.remove(pid);
            auto r = u.pm.renice(pid, q);
            PCB* pcb = u.pm.get_pcb(pid);
            if (pcb && !pcb->suspended && pcb->state != ProcessState::BLOCKED
                && pcb->state != ProcessState::TERMINATED)
                u.mlfq.enqueue_to_level(pid, q);
            return r;
        }

        // ═══ 调度 ═══
        case MessageType::START_SCHED:
            std::cout << "══════════ Start ══════════" << std::endl;
            return u.scheduler->start();
        case MessageType::STOP_SCHED:
            std::cout << "══════════ Stop ══════════" << std::endl;
            return u.scheduler->stop();
        case MessageType::RESTART_SCHED:
            std::cout << "══════════ Restart ══════════" << std::endl;
            return u.scheduler->restart();
        case MessageType::STEP:
            std::cout << "══════════ Step ══════════" << std::endl;
            return u.scheduler->step();

        // ═══ 内存 ═══
        case MessageType::ALLOC: {
            uint32_t size = 0, pid = 0;
            try { size = std::stoul(arg(msg.args, 0)); pid = std::stoul(arg(msg.args, 1)); }
            catch (...) { return "Error: alloc <大小KB> <PID>"; }
            PCB* pcb = u.pm.get_pcb(pid);
            if (!pcb) return "Error: 进程 " + std::to_string(pid) + " 不存在。";
            auto r = u.mm.alloc(size, pid, pcb->name);
            if (r.find("分配成功") != std::string::npos) {
                auto pos = r.find("起始地址="), end = r.find("KB", pos);
                if (pos != std::string::npos && end != std::string::npos) {
                    try {
                        uint32_t addr = std::stoul(r.substr(pos+13, end-pos-13));
                        pcb->mem_block_addrs.push_back(addr);
                        pcb->mem_block_sizes.push_back(size);
                    } catch (...) {}
                }
            }
            return r;
        }
        case MessageType::FREE_MEM: {
            uint32_t addr = 0;
            try { addr = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: 地址无效。"; }
            return u.mm.free_mem(addr);
        }
        case MessageType::SHOW_MEM: return u.mm.show_mem();
        case MessageType::COMPACT: {
            auto m = u.mm.compact();
            for (const auto& cr : m) {
                PCB* pcb = u.pm.get_pcb(cr.pid);
                if (pcb) for (size_t i = 0; i < pcb->mem_block_addrs.size(); ++i)
                    if (pcb->mem_block_addrs[i] == cr.old_addr)
                        pcb->mem_block_addrs[i] = cr.new_addr;
            }
            return "compact 完成。" + std::to_string(m.size()) + " 个块前移。";
        }
        case MessageType::MEM_STAT:       return u.mm.mem_stat();
        case MessageType::SET_ALLOC_ALGO: return u.mm.set_alloc_algo(arg(msg.args, 0));
        case MessageType::PGFAULT:        return u.mm.pgfault();
        case MessageType::SWAP_OUT: {
            uint32_t pid = 0;
            try { pid = std::stoul(arg(msg.args, 0)); } catch (...) { return "Error: PID 无效。"; }
            auto [sz, r] = u.mm.swap_out(pid);
            if (sz > 0) {
                PCB* pcb = u.pm.get_pcb(pid);
                if (pcb) {
                    pcb->swapped_out = true; pcb->swapped_size = sz;
                    pcb->mem_block_addrs.clear(); pcb->mem_block_sizes.clear();
                }
            }
            return r;
        }

        case MessageType::SAVE: { save_state(); return "状态已保存。"; }
        case MessageType::LOAD: { load_state(); return "状态已加载。"; }
        case MessageType::OVERVIEW: {
            std::ostringstream oss;
            oss << "╔══════════════════ System Overview ══════════════════╗" << std::endl;
            oss << u.pm.ptree() << std::endl << std::endl;
            oss << u.mm.show_mem() << std::endl << std::endl;
            oss << "MLFQ Status:" << std::endl;
            oss << u.mlfq.snapshot(&u.pm) << std::endl;
            oss << "╚══════════════════════════════════════════════════════╝";
            return oss.str();
        }
        case MessageType::HELP: return help_text();
        case MessageType::EXIT: return "再见。";
        default: return "Error: 未知命令。";
    }
}

std::string BackendEngine::process(const Message& msg, const std::string& username) {
    if (is_write_cmd(msg.type)) {
        std::unique_lock<std::shared_mutex> lock(process_mtx_);
        std::string result;
        try { result = dispatch(msg, username); }
        catch (...) { result = "Error: 内部错误。"; }
        if (msg.type != MessageType::SAVE) save_state();
        return result;
    } else {
        std::shared_lock<std::shared_mutex> lock(process_mtx_);
        try { return dispatch(msg, username); }
        catch (...) { return "Error: 内部错误。"; }
    }
}
