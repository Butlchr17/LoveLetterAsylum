#include "lla/saves/save_slots.hpp"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lla {
namespace {

struct JsonValue {
    enum class Type { String, Int, Bool };

    Type type = Type::String;
    std::string text;
    int integer = 0;
    bool boolean = false;
};

struct Field {
    std::string key;
    JsonValue value;
};

bool is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool append_code_point(std::string& out, int code) {
    if (code < 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
        return false;
    }
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
    return true;
}

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    bool parse(std::vector<Field>& fields) {
        skip();
        if (!consume('{')) {
            return false;
        }
        skip();
        if (consume('}')) {
            skip();
            return index_ == text_.size();
        }
        while (index_ < text_.size()) {
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            if (!consume(':')) {
                return false;
            }
            JsonValue value;
            if (!parse_value(value)) {
                return false;
            }
            fields.push_back(Field{std::move(key), std::move(value)});
            skip();
            if (consume('}')) {
                skip();
                return index_ == text_.size();
            }
            if (!consume(',')) {
                return false;
            }
        }
        return false;
    }

private:
    void skip() {
        while (index_ < text_.size() && is_space(text_[index_])) {
            ++index_;
        }
    }

    bool consume(char expected) {
        skip();
        if (index_ < text_.size() && text_[index_] == expected) {
            ++index_;
            return true;
        }
        return false;
    }

    bool parse_string(std::string& out) {
        skip();
        if (index_ >= text_.size() || text_[index_] != '"') {
            return false;
        }
        ++index_;
        out.clear();
        while (index_ < text_.size()) {
            const char ch = text_[index_++];
            if (ch == '"') {
                return true;
            }
            if (static_cast<unsigned char>(ch) < 0x20) {
                return false;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (index_ >= text_.size()) {
                return false;
            }
            const char escaped = text_[index_++];
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escaped);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u':
                    if (!parse_unicode(out)) {
                        return false;
                    }
                    break;
                default:
                    return false;
            }
        }
        return false;
    }

    bool parse_unicode(std::string& out) {
        if (index_ + 4 > text_.size()) {
            return false;
        }
        int code = 0;
        for (int nibble = 0; nibble < 4; ++nibble) {
            const char hex = text_[index_++];
            code <<= 4;
            if (hex >= '0' && hex <= '9') {
                code += hex - '0';
            } else if (hex >= 'a' && hex <= 'f') {
                code += hex - 'a' + 10;
            } else if (hex >= 'A' && hex <= 'F') {
                code += hex - 'A' + 10;
            } else {
                return false;
            }
        }
        return append_code_point(out, code);
    }

    bool parse_value(JsonValue& value) {
        skip();
        if (index_ >= text_.size()) {
            return false;
        }
        const char ch = text_[index_];
        if (ch == '"') {
            value.type = JsonValue::Type::String;
            return parse_string(value.text);
        }
        if (ch == '-' || is_digit(ch)) {
            return parse_int(value);
        }
        if (ch == 't') {
            return parse_bool(value, "true", true);
        }
        if (ch == 'f') {
            return parse_bool(value, "false", false);
        }
        return false;
    }

    bool parse_int(JsonValue& value) {
        const std::size_t start = index_;
        if (text_[index_] == '-') {
            ++index_;
        }
        if (index_ >= text_.size() || !is_digit(text_[index_])) {
            return false;
        }
        if (text_[index_] == '0' && index_ + 1 < text_.size() && is_digit(text_[index_ + 1])) {
            return false;
        }
        while (index_ < text_.size() && is_digit(text_[index_])) {
            ++index_;
        }
        int parsed = 0;
        const auto* first = text_.data() + start;
        const auto* last = text_.data() + index_;
        const std::from_chars_result result = std::from_chars(first, last, parsed);
        if (result.ec != std::errc{} || result.ptr != last) {
            return false;
        }
        value.type = JsonValue::Type::Int;
        value.integer = parsed;
        return true;
    }

    bool parse_bool(JsonValue& value, std::string_view literal, bool boolean) {
        if (text_.substr(index_).starts_with(literal) == false) {
            return false;
        }
        index_ += literal.size();
        if (index_ < text_.size()) {
            const char next = text_[index_];
            const bool identifier = is_digit(next) || (next >= 'a' && next <= 'z') ||
                                    (next >= 'A' && next <= 'Z') || next == '_';
            if (identifier) {
                return false;
            }
        }
        value.type = JsonValue::Type::Bool;
        value.boolean = boolean;
        return true;
    }

    std::string_view text_;
    std::size_t index_ = 0;
};

const JsonValue* find_last(const std::vector<Field>& fields, std::string_view key) {
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        if (it->key == key) {
            return &it->value;
        }
    }
    return nullptr;
}

bool valid_slot(int slot) {
    return slot >= 1 && slot <= ISaveSlots::kSlotCount;
}

std::string game_document(const SaveGame& game) {
    return "{\n  \"slot\": " + std::to_string(game.slot) + ",\n  \"started\": true,\n  \"position\": " +
           std::to_string(game.position_millis) + ",\n  \"rank1\": " + std::to_string(game.ranks[0]) + ",\n  \"rank2\": " +
           std::to_string(game.ranks[1]) + ",\n  \"rank3\": " + std::to_string(game.ranks[2]) + ",\n  \"rank4\": " +
           std::to_string(game.ranks[3]) + "\n}\n";
}

bool read_rank(const std::vector<Field>& fields, std::string_view key, int& rank) {
    const JsonValue* value = find_last(fields, key);
    if (value == nullptr) {
        rank = 0;
        return true;
    }
    if (value->type != JsonValue::Type::Int || value->integer < 0) {
        return false;
    }
    rank = value->integer;
    return true;
}

int clamp_position(int position_millis) {
    if (position_millis < 0) {
        return 0;
    }
    if (position_millis > 1000) {
        return 1000;
    }
    return position_millis;
}

void remove_file(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();
    return static_cast<bool>(file);
}

}  // namespace

JsonSaveSlots::JsonSaveSlots(std::filesystem::path directory) : directory_(std::move(directory)) {}

std::filesystem::path JsonSaveSlots::path_for(int slot) const {
    return directory_ / ("slot-" + std::to_string(slot) + ".json");
}

bool JsonSaveSlots::occupied(int slot) const {
    if (!valid_slot(slot)) {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(path_for(slot), error);
}

bool JsonSaveSlots::write_game(const SaveGame& game) {
    if (!valid_slot(game.slot) || !game.started) {
        return false;
    }
    SaveGame stored = game;
    stored.position_millis = clamp_position(game.position_millis);
    const std::filesystem::path destination = path_for(stored.slot);
    const std::filesystem::path temporary(destination.string() + ".tmp");
    try {
        if (!directory_.empty()) {
            std::filesystem::create_directories(directory_);
        }
        if (!write_text(temporary, game_document(stored))) {
            remove_file(temporary);
            return false;
        }
        std::filesystem::rename(temporary, destination);
        return true;
    } catch (const std::filesystem::filesystem_error&) {
        remove_file(temporary);
        return false;
    }
}

bool JsonSaveSlots::start_new_game(int slot) {
    if (!valid_slot(slot)) {
        return false;
    }
    SaveGame game;
    game.slot = slot;
    game.started = true;
    game.position_millis = 500;
    return write_game(game);
}

bool JsonSaveSlots::store_progress(int slot, int position_millis) {
    SaveGame game;
    if (!read_game(slot, game) || !game.started) {
        return false;
    }
    game.position_millis = position_millis;
    return write_game(game);
}

bool JsonSaveSlots::load_party(int slot, int* ranks) const {
    if (ranks == nullptr) {
        return false;
    }
    SaveGame game;
    if (!read_game(slot, game) || !game.started) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        ranks[index] = game.ranks[index];
    }
    return true;
}

bool JsonSaveSlots::store_party(int slot, const int* ranks) {
    if (ranks == nullptr) {
        return false;
    }
    SaveGame game;
    if (!read_game(slot, game) || !game.started) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        if (ranks[index] < 0) {
            return false;
        }
        game.ranks[index] = ranks[index];
    }
    return write_game(game);
}

bool JsonSaveSlots::load_progress(int slot, int& position_millis) const {
    SaveGame game;
    if (!read_game(slot, game) || !game.started) {
        return false;
    }
    position_millis = game.position_millis;
    return true;
}

bool JsonSaveSlots::read_game(int slot, SaveGame& game) const {
    if (!valid_slot(slot)) {
        return false;
    }
    std::ifstream file(path_for(slot), std::ios::binary);
    if (!file) {
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<Field> fields;
    if (!JsonParser(text).parse(fields)) {
        return false;
    }
    const JsonValue* slot_value = find_last(fields, "slot");
    const JsonValue* started_value = find_last(fields, "started");
    if (slot_value == nullptr || slot_value->type != JsonValue::Type::Int || slot_value->integer != slot) {
        return false;
    }
    if (started_value == nullptr || started_value->type != JsonValue::Type::Bool) {
        return false;
    }
    int position_millis = 500;
    if (const JsonValue* position_value = find_last(fields, "position")) {
        if (position_value->type != JsonValue::Type::Int) {
            return false;
        }
        position_millis = clamp_position(position_value->integer);
    }
    int ranks[4] = {};
    if (!read_rank(fields, "rank1", ranks[0]) || !read_rank(fields, "rank2", ranks[1]) || !read_rank(fields, "rank3", ranks[2]) ||
        !read_rank(fields, "rank4", ranks[3])) {
        return false;
    }
    game.slot = slot_value->integer;
    game.started = started_value->boolean;
    game.position_millis = position_millis;
    for (int index = 0; index < 4; ++index) {
        game.ranks[index] = ranks[index];
    }
    return true;
}

}  // namespace lla
