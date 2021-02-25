////////////////////////////////////////////////////////////////////////////////
// run_in_job.cpp
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <cassert>
#include <cstdlib>

#include "Windows.h"

#include "job_object.hpp"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::tuple<std::size_t, std::unique_ptr<std::string>, std::vector<std::unique_ptr<std::string>>> check_args(std::vector<std::unique_ptr<std::string>>& args)
{
    std::tuple<std::size_t, std::unique_ptr<std::string>, std::vector<std::unique_ptr<std::string>>> return_value;
    if (args.size() >= 2)
    {
        // Add memory limit arg
        std::size_t memory_limit_in_mb = std::stoi(*args[0]);
        std::get<0>(return_value) = memory_limit_in_mb;

        // Add program name arg
        std::get<1>(return_value) = std::move(args[1]);

        if (args.size() > 2)
        {
            // Add all other args
            for (std::size_t index = 2; index < args.size(); ++index)
            {
                std::get<2>(return_value).push_back(std::move(args[index]));
            }
        }
    }
    else
    {
        throw std::runtime_error("Incorrect arg count.");
    }

    return return_value;
}

std::vector<std::unique_ptr<std::string>> get_args_from_argv(int argc, char** argv)
{
    std::vector<std::unique_ptr<std::string>> args;

    // Ignore the first arguments (process name)
    for (std::size_t index = 1; index < argc; ++index)
    {
        args.push_back(std::make_unique<std::string>(std::string(argv[index])));
    }

    return args;
}

std::tuple<std::int64_t, ::HANDLE> launch_program(std::unique_ptr<std::string>& program_name, std::vector<std::unique_ptr<std::string>>& args)
{
    ::STARTUPINFOA startup_info;
    ::PROCESS_INFORMATION process_info;

    ::ZeroMemory (&startup_info, sizeof (startup_info));
    startup_info.cb = sizeof (startup_info);
    ::ZeroMemory (&startup_info, sizeof (process_info));

    std::string command_line = *program_name;
    if (args.size() > 0)
    {
        for (std::size_t index = 0; index < args.size(); ++index)
        {
            command_line += " " + *args[index];
        }
    }

    std::cout << "[Proc Start]: " << command_line << std::endl;

    bool success = ::CreateProcessA(
        nullptr,                            // Module Name
        (::LPSTR)command_line.c_str(),      // commandLine
        nullptr,                            // process attributes
        nullptr,                            // thread attributes
        false,                              // inherit handles
        NORMAL_PRIORITY_CLASS,              // creation flags
        nullptr,                            // environment
        nullptr,                            // current directory
        &startup_info,                      // out startup info
        &process_info                       // out process info
    );

    if (!success)
    {
        std::cout << "CreateProcessA failed with the following parameters: " << command_line << std::endl;
        return std::make_tuple(-1, nullptr);
    }

    return std::make_tuple(process_info.dwProcessId, process_info.hProcess);

}

void print_help()
{
    std::cout << "[run_in_job]: Usage" << std::endl;
    std::cout << std::endl;
    std::cout << "run_in_job.exe <memory_cap_in_mb> <program_to_run> <arg0> <arg1> ...";
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    auto&& args = get_args_from_argv(argc, argv);

    std::size_t memory_limits_mb;
    std::unique_ptr<std::string> program_name;
    std::vector<std::unique_ptr<std::string>> program_args;
    try
    {
        auto&& tuple = check_args(args);

        memory_limits_mb = std::get<0>(tuple);
        program_name = std::move(std::get<1>(tuple));
        program_args = std::move(std::get<2>(tuple));
    }
    catch(...)
    {
        return -1;
    }

    auto&& tuple = launch_program(program_name, program_args);

    std::int64_t pid = std::get<0>(tuple);
    ::HANDLE process_handle = std::get<1>(tuple);

    if (pid == -1)
    {
        // Unable to make the process
        assert(!process_handle);

        return -2;
    }

    if (!process_handle)
    {
        // Windows under rare cases may return a null process_handle. We cannot
        // assign the job object if this is the case.

        std::cout << "Process created but win32 returned an invalid handle." << std::endl;
        std::cout << "Will not kill child process. As the handle is incorrect. Please manually kill pid: " << pid << std::endl;
        return -3;
    }

    auto&& second_tuple = launch_program(program_name, program_args);

    std::int64_t second_pid = std::get<0>(second_tuple);
    ::HANDLE second_process_handle = std::get<1>(second_tuple);

    if (second_pid == -1)
    {
        // Unable to make the process
        assert(!process_handle);

        return -2;
    }

    if (!second_process_handle)
    {
        // Windows under rare cases may return a null process_handle. We cannot
        // assign the job object if this is the case.

        std::cout << "Process created but win32 returned an invalid handle." << std::endl;
        std::cout << "Will not kill child process. As the handle is incorrect. Please manually kill pid: " << pid << std::endl;
        return -3;
    }

    std::cout << "Process ID: " << pid << std::endl;

    const std::int64_t lower_bound_limit = 200 * 1024; // 200kb
    const std::int64_t upper_bound_limit = memory_limits_mb * 1024 * 1024;

    ev30::job_object<std::string> job_object("managed job object", upper_bound_limit);

    job_object.create_job_object();
    assert(job_object.exists());

    // Sleep for a second to force the job object to attach AFTER process creation
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool success = job_object.assign_to_process(process_handle);
    assert(job_object.count_of_running_processes_under_job() > 0);
    if (!success)
    {
        // Kill the running process.
        ::TerminateProcess(process_handle, -1);
        std::cout << "Unable to assing job object to the process. Error Code: " << ::GetLastError() << std::endl;
        return -5;
    }

    success = job_object.assign_to_process(second_process_handle);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "Count of assigned processes to job: " << job_object.count_of_running_processes_under_job() << std::endl;

    assert(job_object.count_of_running_processes_under_job() > 1);

    if (!success)
    {
        // Kill the running process.
        ::TerminateProcess(second_process_handle, -1);
        std::cout << "Unable to assing job object to the process. Error Code: " << ::GetLastError() << std::endl;
        return -5;
    }

    std::cout << "Successfully set limits on the job. Press any key to quit." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    assert(job_object.exists());
    assert(job_object.count_of_running_processes_under_job() == 0);

    return 0;
}