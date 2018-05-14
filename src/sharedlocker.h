#pragma once

#include <shared_mutex>
#include <type_traits>

template <class T, typename E = void>
class SharedLocker;

template <class T>
class SharedLocker<T, std::enable_if_t<!std::is_pointer_v<T>>>
{
public:
    SharedLocker(SharedLocker&&) = default;
    explicit SharedLocker(std::add_pointer_t<T> obj, std::shared_mutex* mutex)
        : m_obj(obj), m_mutex(mutex)
    {
        if (nullptr == m_obj || nullptr == m_mutex)
        {
            throw std::runtime_error("Invalid arguments");
        }

        m_mutex->lock_shared();
    }

    ~SharedLocker()
    {
        m_mutex->unlock_shared();
    }

    const std::add_lvalue_reference_t<T> operator*() { return *m_obj; }
    const std::add_pointer_t<T>          operator->() { return m_obj; }

private:
    SharedLocker()                    = delete;
    SharedLocker(const SharedLocker&) = delete;
    SharedLocker& operator=(const SharedLocker&) = delete;
    SharedLocker& operator=(SharedLocker&&) = delete;

    const std::add_pointer_t<T> m_obj;
    std::shared_mutex*          m_mutex;
};

template <class T>
inline SharedLocker<T, std::enable_if_t<!std::is_pointer_v<T>>>
make_shared_locker(std::add_pointer_t<T> t, std::shared_mutex* u)
{
    return (SharedLocker<T>(t, u));
}
