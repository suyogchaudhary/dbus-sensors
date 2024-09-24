#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>

using Value = sdbusplus::xyz::openbmc_project::Sensor::server::Value;

class CpuUtilization : public sdbusplus::server::object::object<Value>
{
  public:
    CpuUtilization(sdbusplus::bus::bus& bus, const char* path) :
        sdbusplus::server::object::object<Value>(bus, path),
        bus(bus)
    {
        // Start a thread that will periodically update CPU utilization
        updateThread = std::thread([this]() {
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::seconds(60));
                update();
            }
        });
        updateThread.detach();
    }

    // Override the setter method for the value
    double value(double newValue) override
    {
        return Value::value(newValue);
    }

    // Override the getter method for the value
    double value() const override
    {
        return Value::value();
    }

  private:
    sdbusplus::bus::bus& bus;
    std::thread updateThread;

    void update()
    {
        double cpuUsage = getCpuUsage();
        value(cpuUsage);
        std::cout << "CPU Utilization updated: " << cpuUsage << "%" << std::endl;
    }

    double getCpuUsage()
    {
        std::ifstream statFile("/proc/stat");
        std::string line;
        std::getline(statFile, line);
        statFile.close();

        unsigned long long user, nice, system, idle;
        sscanf(line.c_str(), "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);

        static unsigned long long prevTotal = 0, prevIdle = 0;
        unsigned long long total = user + nice + system + idle;
        unsigned long long diffTotal = total - prevTotal;
        unsigned long long diffIdle = idle - prevIdle;

        double cpuUsage = (1.0 - (double)diffIdle / diffTotal) * 100.0;

        prevTotal = total;
        prevIdle = idle;

        return cpuUsage;
    }
};

int main()
{
    auto bus = sdbusplus::bus::new_default();
    sdbusplus::server::manager::manager objManager(bus, "/xyz/openbmc_project/sensors");
    CpuUtilization cpuUtilization(bus, "/xyz/openbmc_project/sensors/utilization/cpu0");

    bus.request_name("xyz.openbmc_project.CpuUtilization");

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }

    return 0;
}

