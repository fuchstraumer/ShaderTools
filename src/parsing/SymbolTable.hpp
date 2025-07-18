#pragma once
#ifndef SHADERTOOLS_SYMBOL_TABLE_HPP
#define SHADERTOOLS_SYMBOL_TABLE_HPP
#include "common/CommonInclude.hpp"
#include <unordered_map>
#include <string>
#include <format>
#include <optional>

namespace st
{

    /**
     * The typology here is rather specific, as it's intended to help us identify the key types of symbols we care
     * about in this library - like interface variables, uniforms, and so on. This is less generic than most
     * true symbol tables.
     */
    enum class SymbolType : uint8_t
    {
        Identifier,
        Variable,
        Function,
        InputInterfaceBlock,
        InputInterfaceVariable,
        OutputInterfaceBlock,
        OutputInterfaceVariable,
        ResourceBlock,
        ResourceBlockVariable,
        Invalid,

    };

    /**
     * @brief Represents a single symbol in the symbol table.
     */
    struct Symbol
    {
        constexpr Symbol() noexcept;
        constexpr Symbol(
            SymbolType type,
            std::string name,
            std::string type_name,
            std::string_view parent_scope,
            std::string_view parent_type_name) noexcept;
        ~Symbol() noexcept = default;
        constexpr Symbol(const Symbol&) noexcept;
        constexpr Symbol(Symbol&&) noexcept;
        constexpr Symbol& operator=(const Symbol&) noexcept;
        constexpr Symbol& operator=(Symbol&&) noexcept;

        SymbolType Type = SymbolType::Invalid;
        std::string Name;
        std::string TypeName;
        std::string_view ParentScope;
        std::string_view ParentTypeName;

        constexpr bool IsMemberVariable() const noexcept
        {
            return isVariableType() && !ParentScope.empty();
        }

        std::string GetQualifiedName() const noexcept
        {
            if (ParentScope.empty())
            {
                return Name;
            }
            else
            {
                return std::format("{}.{}", ParentScope, Name);
            }
        }
    
    private:

        constexpr bool isVariableType() const noexcept
        {
            return Type == SymbolType::Variable ||
                   Type == SymbolType::InputInterfaceVariable ||
                   Type == SymbolType::OutputInterfaceVariable ||
                   Type == SymbolType::ResourceBlockVariable;
        }
    };
    
    /**
     * @brief Basic symbol table, which merges symbols generated from the configuration files with individual shader stages we parse. 
     * The configuration file symbols serve as "global" symbols referred to by the source code, which is kept scoped. Extracting symbols
     * for each source code - representing a single shader stage - also allows us to keep track of interface-related symbols, such as 
     * vertex shader output or whatever the fragment shader wishes to write.
     * 
     * This is primitive and stupid and could work better and be more performant, but it works for now and gets the job done. May revisit
     * if it proves a particular bottleneck, but I really doubt it.
     */
    class SymbolTable
    {
    public:


        void AddHierarchicalSymbol(Symbol symbol, std::vector<Symbol> children);
        void AddSymbol(Symbol symbol);

        Symbol* Find(const std::string& name, std::optional<std::string_view> scope = std::nullopt) const;
        Symbol* FindUsingQualifiedName(const std::string& qualified_name) const;
    
    private:

        /** Each new instance of a symbol table can inherit symbols from a parent, representing descending scopes. */
        std::weak_ptr<SymbolTable> parentTable;
        std::unordered_map<std::string, Symbol> symbols;
        std::unordered_map<std::string, std::unique_ptr<SymbolTable>> childTables;

        
    };


}

#endif // !SHADERTOOLS_SYMBOL_TABLE_HPP
