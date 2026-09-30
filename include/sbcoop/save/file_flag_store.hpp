#pragma once

#include "sbcoop/save/save_system.hpp"

#include <filesystem>

namespace sbcoop::save {

// Stores only mod metadata. No method accepts a native save path or writes .sav.
// Pending intents persist through interruption and never count as co-op flags.
class FileFlagStore final : public IFlagStore {
public:
    explicit FileFlagStore(std::filesystem::path directory);
    std::vector<Flag> read_all() const override;
    bool write_intent(const CreationRequest& request) override;
    bool commit_created(const Flag& flag) override;

private:
    std::filesystem::path directory_;
};

} // namespace sbcoop::save
