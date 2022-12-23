#include "common/UtilityStructs.hpp"

namespace st
{

    dll_retrieved_strings_t::dll_retrieved_strings_t() {}

    dll_retrieved_strings_t::~dll_retrieved_strings_t()
    {
        FreeMemory();
    }

    dll_retrieved_strings_t::dll_retrieved_strings_t(dll_retrieved_strings_t&& other) noexcept : NumStrings(std::move(other.NumStrings)), Strings(std::move(other.Strings))
    {
        other.NumStrings = 0;
        other.Strings = nullptr;
    }

    dll_retrieved_strings_t& dll_retrieved_strings_t::operator=(dll_retrieved_strings_t&& other) noexcept
    {
        NumStrings = std::move(other.NumStrings);
        other.NumStrings = 0;
        Strings = std::move(other.Strings);
        other.Strings = nullptr;
        return *this;
    }

    void dll_retrieved_strings_t::SetNumStrings(const size_t & num_names)
    {
        // Unlikely that we get here with allocated memory existing, but better safe than sorry (leaking weird memory)
        FreeMemory();
        NumStrings = num_names;
        Strings = new char*[num_names];
    }

    void dll_retrieved_strings_t::FreeMemory()
    {

        for (uint32_t i = 0; i < NumStrings; ++i)
        {
            free(Strings[i]);
        }

        if (Strings != nullptr)
        {
            delete[] Strings;
            Strings = nullptr;
        }

    }

    const char* dll_retrieved_strings_t::operator[](const size_t& idx) const
    {
        return Strings[idx];
    }

    ShaderResourceSubObject::ShaderResourceSubObject(const ShaderResourceSubObject& other) noexcept :
        Type(strdup(other.Type)),
        Name(strdup(other.Name)),
        NumElements(other.NumElements),
        isComplex(other.isComplex),
        Offset(other.Offset),
        Size(other.Size) {}

    ShaderResourceSubObject::ShaderResourceSubObject(ShaderResourceSubObject&& other) noexcept : 
        Type(std::move(other.Type)),
        Name(std::move(other.Name)),
        NumElements(std::move(other.NumElements)),
        isComplex(std::move(other.isComplex)),
        Offset(std::move(other.Offset)),
        Size(std::move(other.Size))
    {
        other.Type = nullptr;
        other.Name = nullptr;
    }

    ShaderResourceSubObject& ShaderResourceSubObject::operator=(const ShaderResourceSubObject& other) noexcept
    {
        Type = strdup(other.Type);
        Name = strdup(other.Name);
        Offset = other.Offset;
        Size = other.Size;
        NumElements = other.NumElements;
        isComplex = other.isComplex;
        return *this;
    }

    ShaderResourceSubObject& ShaderResourceSubObject::operator=(ShaderResourceSubObject&& other) noexcept
    {
        Type = std::move(other.Type);
        Name = std::move(other.Name);
        NumElements = std::move(other.NumElements);
        isComplex = std::move(other.isComplex);
        Size = std::move(other.Size);
        Offset = std::move(other.Offset);
        other.Type = nullptr;
        other.Name = nullptr;
        return *this;
    }

    ShaderResourceSubObject::~ShaderResourceSubObject()
    {
        if (Type != nullptr)
        {
            free(Type);
        }

        if (Name != nullptr)
        {
            free(Name);
        }
    }

}
