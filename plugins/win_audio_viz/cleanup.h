#pragma once

class AudioClientStopOnExit
{
public:
    AudioClientStopOnExit(IAudioClient* p)
        : m_p(p) {}
    ~AudioClientStopOnExit()
    {
        HRESULT hr = m_p->Stop();
        if (FAILED(hr))
        {
            warn("IAudioClient::Stop failed: hr = 0x%08lx", hr);
        }
    }

private:
    IAudioClient* m_p;
};

class AvRevertMmThreadCharacteristicsOnExit
{
public:
    AvRevertMmThreadCharacteristicsOnExit(HANDLE hTask)
        : m_hTask(hTask) {}
    ~AvRevertMmThreadCharacteristicsOnExit()
    {
        if (!AvRevertMmThreadCharacteristics(m_hTask))
        {
            warn("AvRevertMmThreadCharacteristics failed: last error is %lu", GetLastError());
        }
    }

private:
    HANDLE m_hTask;
};

class CancelWaitableTimerOnExit
{
public:
    CancelWaitableTimerOnExit(HANDLE h)
        : m_h(h) {}
    ~CancelWaitableTimerOnExit()
    {
        if (!CancelWaitableTimer(m_h))
        {
            warn("CancelWaitableTimer failed: last error is %lu", GetLastError());
        }
    }

private:
    HANDLE m_h;
};

class CloseHandleOnExit
{
public:
    CloseHandleOnExit(HANDLE h)
        : m_h(h) {}
    ~CloseHandleOnExit()
    {
        if (!CloseHandle(m_h))
        {
            warn("CloseHandle failed: last error is %lu", GetLastError());
        }
    }

private:
    HANDLE m_h;
};

class CoTaskMemFreeOnExit
{
public:
    CoTaskMemFreeOnExit(PVOID p)
        : m_p(p) {}
    ~CoTaskMemFreeOnExit()
    {
        CoTaskMemFree(m_p);
    }

private:
    PVOID m_p;
};

class CoUninitializeOnExit
{
public:
    ~CoUninitializeOnExit()
    {
        CoUninitialize();
    }
};

class PropVariantClearOnExit
{
public:
    PropVariantClearOnExit(PROPVARIANT* p)
        : m_p(p) {}
    ~PropVariantClearOnExit()
    {
        HRESULT hr = PropVariantClear(m_p);
        if (FAILED(hr))
        {
            warn("PropVariantClear failed: hr = 0x%08lx", hr);
        }
    }

private:
    PROPVARIANT* m_p;
};

class ReleaseOnExit
{
public:
    ReleaseOnExit(IUnknown* p)
        : m_p(p) {}
    ~ReleaseOnExit()
    {
        m_p->Release();
    }

private:
    IUnknown* m_p;
};

class SetEventOnExit
{
public:
    SetEventOnExit(HANDLE h)
        : m_h(h) {}
    ~SetEventOnExit()
    {
        if (!SetEvent(m_h))
        {
            warn("SetEvent failed: last error is %lu", GetLastError());
        }
    }

private:
    HANDLE m_h;
};

class WaitForSingleObjectOnExit
{
public:
    WaitForSingleObjectOnExit(HANDLE h)
        : m_h(h) {}
    ~WaitForSingleObjectOnExit()
    {
        DWORD dwWaitResult = WaitForSingleObject(m_h, INFINITE);
        if (WAIT_OBJECT_0 != dwWaitResult)
        {
            warn("WaitForSingleObject returned unexpected result 0x%08lx, last error is %lu", dwWaitResult, GetLastError());
        }
    }

private:
    HANDLE m_h;
};
