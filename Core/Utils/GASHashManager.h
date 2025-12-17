#pragma once
#pragma once
#include <cstdint>
#include <cstddef>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

uint64_t CalculateXXHash64(const void* Data, size_t Length, uint64_t Seed = 0);

uint64_t GenerateGUID64(const std::string& InString);
