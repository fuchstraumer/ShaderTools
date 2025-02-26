#include "common/ShaderToolsErrors.hpp"

namespace st
{
    namespace detail
    {
        // An issue with the actual content of what we're processing, thus category encompassses all issues
        // with input data
        class InputDataCategory : public std::error_category
        {
        public:
            const char* name() const noexcept override
            {
                return "ShaderToolsInputData";
            }

            std::string message(int ev) const override;
        };

        class InternalErrorCategory : public std::error_category
        {
        public:
            const char* name() const noexcept override
            {
                return "ShaderToolsInternal";
            }

            std::string message(int ev) const override;
        };

        // A ton of our failures are due to filesystem issues, either invalid paths or files we can't open
        class FilesystemCategory : public std::error_category
        {
        public:
            const char* name() const noexcept override
            {
                return "ShaderToolsFilesystem";
            }

            std::string message(int ev) const override;
        };

    } // namespace detail
    
    const char* ErrorCodeToText(ShaderToolsErrorCode code) noexcept
    {
        return "null_string";
    }

	const char* ErrorSourceToTest(ShaderToolsErrorSource source) noexcept
	{
        return "null_string";
	}

}
