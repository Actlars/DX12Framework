#pragma once

//-----------------------------------------------------------------------------
//      nullptrを考慮してdelete[]
//-----------------------------------------------------------------------------
template<typename T>
inline void SafeDelete(T*& ptr)
{
    if (ptr != nullptr)
    {
        delete ptr;
        ptr = nullptr;
    }
}

//-----------------------------------------------------------------------------
//      nullptrを考慮してdelete[]
//-----------------------------------------------------------------------------
template<typename T>
inline void SafeDeleteArray(T*& ptr)
{
    if (ptr != nullptr)
    {
        delete[] ptr;
        ptr = nullptr;
    }
}

//-----------------------------------------------------------------------------
//      nullptrを考慮してRelease()メソッドを呼び出し
//-----------------------------------------------------------------------------
template<typename T>
inline void SafeRelease(T*& ptr)
{
    if (ptr != nullptr)
    {
        ptr->Release();
        ptr = nullptr;
    }
}

//-----------------------------------------------------------------------------
//      nullptrを考慮してTerm()メソッドを呼び出し,delete
//-----------------------------------------------------------------------------
template<typename T>
inline void SafeTerm(T*& ptr)
{
    if (ptr != nullptr)
    {
        ptr->Term();
        delete ptr;
        ptr = nullptr;
    }
}
