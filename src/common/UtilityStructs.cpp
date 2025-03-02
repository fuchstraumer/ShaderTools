#include "common/UtilityStructs.hpp"

namespace st
{

    dll_retrieved_strings_t::dll_retrieved_strings_t() {}

    dll_retrieved_strings_t::~dll_retrieved_strings_t()
    {
        FreeMemory();
    }

    dll_retrieved_strings_t::dll_retrieved_strings_t(dll_retrieved_strings_t&& other) noexcept : NumStrings(other.NumStrings), Strings(other.Strings)
    {
        other.NumStrings = 0;
        other.Strings = nullptr;
    }

    dll_retrieved_strings_t& dll_retrieved_strings_t::operator=(dll_retrieved_strings_t&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        // Clean up existing resources
        FreeMemory();

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

    char* CopyString(const char* string_to_copy)
    {
        if (string_to_copy == nullptr)
        {
            return nullptr;
        }
        char* new_string = new char[std::strlen(string_to_copy) + 1];
        std::strcpy(new_string, string_to_copy);
        return new_string;
    }

    ShaderResourceSubObject::ShaderResourceSubObject(const ShaderResourceSubObject& other) noexcept :
        Type(CopyString(other.Type)),
        Name(CopyString(other.Name)),
        NumElements(other.NumElements),
        isComplex(other.isComplex),
        Offset(other.Offset),
        Size(other.Size)
    {

    }

    ShaderResourceSubObject::ShaderResourceSubObject(ShaderResourceSubObject&& other) noexcept :
        Type(other.Type),
        Name(other.Name),
        NumElements(other.NumElements),
        isComplex(other.isComplex),
        Offset(other.Offset),
        Size(other.Size)
    {
        other.Type = nullptr;
        other.Name = nullptr;
    }

    ShaderResourceSubObject& ShaderResourceSubObject::operator=(const ShaderResourceSubObject& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        SetType(other.Type);
        SetName(other.Name);
        Offset = other.Offset;
        Size = other.Size;
        NumElements = other.NumElements;
        isComplex = other.isComplex;
        return *this;
    }

    ShaderResourceSubObject& ShaderResourceSubObject::operator=(ShaderResourceSubObject&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        
        // Clean up existing resources
        if (Type != nullptr)
        {
            delete[] Type;
        }
        
        if (Name != nullptr)
        {
            delete[] Name;
        }
        
        Type = other.Type;
        Name = other.Name;
        NumElements = other.NumElements;
        isComplex = other.isComplex;
        Size = other.Size;
        Offset = other.Offset;
        other.Type = nullptr;
        other.Name = nullptr;
        return *this;
    }

    ShaderResourceSubObject::~ShaderResourceSubObject()
    {
        if (Type != nullptr)
        {
            delete[] Type;
        }

        if (Name != nullptr)
        {
            delete[] Name;
        }
    }

    void ShaderResourceSubObject::SetName(const char* name)
    {
        if (Name != nullptr)
        {
            delete[] Name;
        }
        Name = CopyString(name);
    }

    void ShaderResourceSubObject::SetType(const char* type)
    {
        if (Type != nullptr)
        {
            delete[] Type;
        }
        Type = CopyString(type);
    }

    void SetSpecializationConstantValue(SpecializationConstant& constant, const SpecializationConstant& other)
    {
        switch (constant.Type)
        {
        case SpecializationConstant::constant_type::b32:
            constant.value_b32 = other.value_b32;
            break;
        case SpecializationConstant::constant_type::f32:
            constant.value_f32 = other.value_f32;
            break;
        case SpecializationConstant::constant_type::f64:
            constant.value_f64 = other.value_f64;
            break;
        case SpecializationConstant::constant_type::i32:
            constant.value_i32 = other.value_i32;
            break;
        case SpecializationConstant::constant_type::i64:
            constant.value_i64 = other.value_i64;
            break;
        case SpecializationConstant::constant_type::ui32:
            constant.value_ui32 = other.value_ui32;
            break;
        case SpecializationConstant::constant_type::ui64:
            constant.value_ui64 = other.value_ui64;
            break;
        case SpecializationConstant::constant_type::invalid:
            break;
        }
    }

    SpecializationConstant::~SpecializationConstant()
    {
        if (Name != nullptr)
        {
            delete[] Name;
        }
    }

    SpecializationConstant::SpecializationConstant(const SpecializationConstant& other) noexcept :
        Type(other.Type),
        ConstantID(other.ConstantID),
        Name(CopyString(other.Name))
    {
        SetSpecializationConstantValue(*this, other);
    }

    SpecializationConstant::SpecializationConstant(SpecializationConstant&& other) noexcept :
        Type(other.Type),
        ConstantID(other.ConstantID),
        Name(other.Name)
    {
        SetSpecializationConstantValue(*this, other);
        other.Name = nullptr;
    }

    SpecializationConstant& SpecializationConstant::operator=(const SpecializationConstant& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        SetName(other.Name);
        Type = other.Type;
        ConstantID = other.ConstantID;
        SetSpecializationConstantValue(*this, other);
        return *this;
    }

    SpecializationConstant& SpecializationConstant::operator=(SpecializationConstant&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        
        // Clean up existing resources
        if (Name != nullptr)
        {
            delete[] Name;
        }
        
        Name = other.Name;
        Type = other.Type;
        ConstantID = other.ConstantID;
        SetSpecializationConstantValue(*this, other);
        other.Name = nullptr;
        return *this;
    }

    void SpecializationConstant::SetName(const char* name)
    {
        if (Name != nullptr)
        {
            delete[] Name;
        }
        Name = CopyString(name);
    }

}
