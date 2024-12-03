#include "parser/parser_base.hpp"
#include <cstring>

namespace catchy::parser {

std::string ParserBase::extract_node_text(const TSNode &node, const std::string &source_code) {
    uint32_t start_byte = ts_node_start_byte(node);
    uint32_t end_byte = ts_node_end_byte(node);
    
    if (start_byte > end_byte || end_byte > source_code.length()) {
        return "";
    }
    
    return source_code.substr(start_byte, end_byte - start_byte);
}

std::optional<std::string> ParserBase::get_function_name(const TSNode &node) {
    if (ts_node_is_null(node)) {
        return std::nullopt;
    }

    const char* type = ts_node_type(node);
    if (strcmp(type, "identifier") == 0) {
        return extract_node_text(node, "");  // TODO: Need source code here
    }

    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; ++i) {
        TSNode child = ts_node_child(node, i);
        auto result = get_function_name(child);
        if (result) {
            return result;
        }
    }

    return std::nullopt;
}

} // namespace catchy::parser
