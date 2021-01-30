////////////////////////////////////////////////////////////////////////////////
// run_in_job.cpp
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <cassert>
#include "cstdlib"
#include "Windows.h"

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

::HANDLE create_job_object(const std::int64_t lower_bound_limit, const std::int64_t upper_bound_limit)
{
    ::HANDLE job_object_handle = ::CreateJobObject(
        nullptr,                                        // security attribs
        (LPCSTR)L"experimental job object"              // name
    );

    if (job_object_handle == nullptr)
    {
        std::cout << "Unable to create a job object. Error Code: " << ::GetLastError() << std::endl;
        return nullptr;
    }
    
    ::JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;

    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY | JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_WORKINGSET;

    // Memory limits
    limits.JobMemoryLimit = upper_bound_limit;
    limits.ProcessMemoryLimit = (upper_bound_limit / 1);

    // Both the lower bound and upper bound are required. If not passed the OS will
    // return 0x57.
    limits.BasicLimitInformation.MaximumWorkingSetSize = (upper_bound_limit / 1);

    // 200k is the lowest possible value.
    assert(lower_bound_limit >= 200 * 1024);
    limits.BasicLimitInformation.MinimumWorkingSetSize = lower_bound_limit;

    bool success = ::SetInformationJobObject(
        job_object_handle,                              // job object handle
        ::JobObjectExtendedLimitInformation,            // JobObjectInformationClass
        &limits,                                        // limits
        sizeof(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION)  // JobObjectInformationClass size
    );

    if (!success)
    {
        std::cout << "Unalbe to set limits: Error Code: " << ::GetLastError() << std::endl;
        return nullptr;
    }

    return job_object_handle;
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

    std::cout << "Process ID: " << pid << std::endl;

    const std::int64_t lower_bound_limit = 200 * 1024; // 200kb
    const std::int64_t upper_bound_limit = memory_limits_mb * 1024 * 1024;
    ::HANDLE job_object_handle = create_job_object(lower_bound_limit, upper_bound_limit);

    if (!job_object_handle)
    {
        std::cout << "Failure creating the job object." << std::endl;
        return -4;
    }

    // Add a breakpoint here to assign the job object to the process after process
    // creation.

    bool success = ::AssignProcessToJobObject(job_object_handle, process_handle);

    if (!success)
    {
        // Kill the running process.
        ::TerminateProcess(process_handle, -1);
        std::cout << "Unable to assing job object to the process. Error Code: " << ::GetLastError() << std::endl;
        return -5;
    }

    std::cout << "Successfully set limits on the job. Press any key to quit." << std::endl;

    char input;
    std::cin >> input;

    return 0;
}