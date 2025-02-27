// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once

#include "../Array/ArrayData.hpp"
#include "../Numbers/JsonFloat.hpp"
#include "../Numbers/JsonInteger.hpp"
#include "../Object/ObjectData.hpp"

ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE

template <typename TResult>
struct VariantDataVisitor {
  typedef TResult result_type;

  template <typename T>
  TResult visit(const T&) {
    return TResult();
  }
};

ARDUINOJSON_END_PRIVATE_NAMESPACE
