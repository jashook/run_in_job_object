////////////////////////////////////////////////////////////////////////////////
// Module: job_object.hpp
// 
// Notes:
//
// Simple class to abstract the job object creation
////////////////////////////////////////////////////////////////////////////////

#ifndef __JOB_OBJECT_HPP__
#define __JOB_OBJECT_HPP__

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include "Windows.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace ev30 {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

template<typename __Type> class job_object
{
    private: // Member variables

        std::size_t _m_assigned_process_count;
        ::HANDLE _m_job_object_handle;
        __Type _m_job_object_name;
        std::size_t _m_process_memory_limit;

    public: // Ctor | Dtor
        job_object(const __Type& name, std::size_t process_memory_limit) { this->_ctor(name, process_memory_limit); }
        ~job_object() { this->_dtor(); }

    public: // Member functions
        bool assign_to_process(::HANDLE process_handle) { return this->_assign_to_process(process_handle); }
        std::size_t count_of_running_processes_under_job() { return this->_count_of_running_processes_under_job(); }
        void create_job_object() { this->_create_job_object(); }
        bool create_job_object_and_assign_to_process(::HANDLE process_handle) { this->_create_job_object(); return this->assign_to_process(process_handle); }
        bool exists() { return this->_check_if_job_object_exists(); }

    private: // Ctor | Dtor

        void _ctor(const __Type& name, std::size_t process_memory_limit)
        {
            this->_m_job_object_name = name;
            this->_m_assigned_process_count = 0;
            this->_m_job_object_handle = nullptr;
            this->_m_process_memory_limit = process_memory_limit;
        }

        void _dtor()
        {
            this->_m_assigned_process_count = 0;

            if (this->_m_job_object_handle)
            {
                ::CloseHandle(this->_m_job_object_handle);
            }
        }

    private: // Private member functions
        bool _assign_to_process(::HANDLE process_handle)
        {
            assert(this->_check_if_job_object_exists());

            bool success = false;

            std:size_t count_of_assinged_processes = this->_count_of_running_processes_under_job();
            if (count_of_assinged_processes > 0)
            {
                // Adjust the job limits to correctly account for per process
                // limits.
                
                ::JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits;
                success = this->_get_job_object_limits(job_limits);

                job_limits.JobMemoryLimit = job_limits.ProcessMemoryLimit * (count_of_assinged_processes + 1);

                success = ::SetInformationJobObject(
                    this->_m_job_object_handle,                     // job object handle
                    ::JobObjectExtendedLimitInformation,            // JobObjectInformationClass
                    &job_limits,                                    // limits
                    sizeof(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION)  // JobObjectInformationClass size
                );

                if (!success)
                {
                    return false;
                }
            }
            
            success = ::AssignProcessToJobObject(this->_m_job_object_handle, process_handle);
            return success;
        }

        bool _check_if_job_object_exists()
        {
            bool success = false;

            // Query the job object based on its name
            // HANDLE OpenJobObjectA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
            ::HANDLE queried_job_object = ::OpenJobObjectA(JOB_OBJECT_ALL_ACCESS, false, static_cast<LPCSTR>(this->_m_job_object_name.c_str()));

            if (!queried_job_object)
            {
                return success;
            }

            ::CloseHandle(queried_job_object);
            return true;
        }

        std::size_t _count_of_running_processes_under_job()
        {
            bool success = true;

            // Query the job object based on its name
            // HANDLE OpenJobObjectA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
            ::HANDLE queried_job_object = ::OpenJobObjectA(JOB_OBJECT_ALL_ACCESS, false, static_cast<LPCSTR>(this->_m_job_object_name.c_str()));

            if (!queried_job_object)
            {
                return 0;
            }

            ::JOBOBJECT_BASIC_PROCESS_ID_LIST pid_list;
            std::memset(&pid_list, 0, sizeof(::JOBOBJECT_BASIC_PROCESS_ID_LIST));

            ::DWORD return_length = 0;
            success = ::QueryInformationJobObject(queried_job_object, 
                                                             ::JobObjectBasicProcessIdList, 
                                                             &pid_list, 
                                                             sizeof(::JOBOBJECT_BASIC_PROCESS_ID_LIST), 
                                                             &return_length);

            // If this is not because we have more process ids than the default
            // buffer can hold
            if (!success && ::GetLastError() != 234)
            {
                std::cerr << "Unable to QueryInformationJobObject: " << ::GetLastError() << std::endl;
                return 0;
            }
            else
            {
                // Ignore the error. We have the number of assigned processes which
                // is valid.
            }

            ::CloseHandle(queried_job_object);
            return pid_list.NumberOfAssignedProcesses;
        }

        void _create_job_object()
        {
            assert(!this->_check_if_job_object_exists());

            // CreateJobObject(LPSECURITY_ATTRIBUTES lpJobAttributes, LPCSTR lpName)
            this->_m_job_object_handle = ::CreateJobObject(nullptr, static_cast<LPCSTR>(this->_m_job_object_name.c_str()));

            if (this->_m_job_object_handle == nullptr)
            {
                std::cout << "Unable to create a job object. Error Code: " << ::GetLastError() << std::endl;
            }
            
            ::JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
            std::memset(&limits, 0, sizeof(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

            // We will assign JOB_OBJECT_LIMIT_JOB_MEMORY, which is the overall memory limit for the job object.
            // This includes the limit for multiple processes. At the same time we will set JOB_OBJECT_LIMIT_PROCESS_MEMORY
            // to limit each individual process.
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY | JOB_OBJECT_LIMIT_PROCESS_MEMORY;

            // Memory limits
            // Assume we only have one process that will be assinged to the job
            // object. If this changes, we will re-evaluate the limits at attach
            // time.
            limits.JobMemoryLimit = this->_m_process_memory_limit;
            limits.ProcessMemoryLimit = this->_m_process_memory_limit;

            // Both the lower bound and upper bound are required. If not passed the OS will
            // return 0x57.
            // limits.BasicLimitInformation.MaximumWorkingSetSize = (upper_bound_limit / 1);

            // // 200k is the lowest possible value.
            // assert(lower_bound_limit >= 200 * 1024);
            // limits.BasicLimitInformation.MinimumWorkingSetSize = lower_bound_limit;

            bool success = ::SetInformationJobObject(
                this->_m_job_object_handle,                     // job object handle
                ::JobObjectExtendedLimitInformation,            // JobObjectInformationClass
                &limits,                                        // limits
                sizeof(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION)  // JobObjectInformationClass size
            );

            if (!success)
            {
                std::cout << "Unable to set limits: Error Code: " << ::GetLastError() << std::endl;
            }
        }

        bool _get_job_object_limits(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION& queried_limits)
        {
            bool success = true;

            // Query the job object based on its name
            // HANDLE OpenJobObjectA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
            ::HANDLE queried_job_object = ::OpenJobObjectA(JOB_OBJECT_ALL_ACCESS, false, static_cast<LPCSTR>(this->_m_job_object_name.c_str()));

            if (!queried_job_object)
            {
                return false;
            }

            std::memset(&queried_limits, 0, sizeof(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

            ::DWORD return_length = 0;
            success = ::QueryInformationJobObject(queried_job_object, 
                                                  ::JobObjectExtendedLimitInformation, 
                                                  &queried_limits, 
                                                  sizeof(::JOBOBJECT_EXTENDED_LIMIT_INFORMATION), 
                                                  &return_length);

            if (!success)
            {
                success = false;
            }

            ::CloseHandle(queried_job_object);
            return success;
        }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

} // end of namespace ev30

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#endif // __JOB_OBJECT_HPP__

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////