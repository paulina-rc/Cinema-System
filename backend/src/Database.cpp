#include "Database.h"

struct Database::Impl {};

Database::Database(const std::string&) : impl_(nullptr) {}
Database::~Database() { delete impl_; }
bool Database::isOpen() const noexcept { return false; }
