// Copyright � 2010-2015 The CefSharp Project. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include "Stdafx.h"
#include "V8Serialization.h"
#include "JavascriptCallbackRegistry.h"
#include "../CefSharp.Core/Internals/Serialization/Primitives.h"

#include <deque>

using namespace std;

namespace CefSharp
{
    namespace Internals
    {
        namespace Serialization
        {
            typedef deque<CefRefPtr<CefV8Value>> value_deque;

            template<typename TList, typename TIndex>
            void SerializeV8Object(CefRefPtr<CefV8Value> obj, CefRefPtr<TList> list, TIndex index, JavascriptCallbackRegistry^ callbackRegistry, value_deque &seen)
            {
                for (value_deque::const_iterator it = seen.begin(); it != seen.end(); ++it)
                {
                    if (obj->IsSame(*it))
                    {
                        throw exception("Cycle found");
                    }
                }
                seen.push_back(obj);

                if (obj->IsNull() || obj->IsUndefined())
                {
                    list->SetNull(index);
                }
                else if (obj->IsBool())
                    list->SetBool(index, obj->GetBoolValue());
                else if (obj->IsInt())
                    list->SetInt(index, obj->GetIntValue());
                else if (obj->IsDouble())
                    list->SetDouble(index, obj->GetDoubleValue());
                else if (obj->IsString())
                    list->SetString(index, obj->GetStringValue());
                else if (obj->IsDate())
                    SetCefTime(obj->GetDateValue(), list, index);
                else if (obj->IsArray())
                {
                    int arrLength = obj->GetArrayLength();
                    std::vector<CefString> keys;
                    if (arrLength > 0 && obj->GetKeys(keys))
                    {
                        auto array = CefListValue::Create();
                        for (int i = 0; i < arrLength; i++)
                        {
                            SerializeV8Object(obj->GetValue(keys[i]), array, i, callbackRegistry, seen);
                        }

                        list->SetList(index, array);
                    }
                    else
                    {
                        list->SetNull(index);
                    }
                }
                else if (obj->IsFunction())
                {
                    auto context = CefV8Context::GetCurrentContext();
                    auto jsCallback = callbackRegistry->Register(context, obj);
                    SetJsCallback(jsCallback, list, index);
                }
                else if (obj->IsObject())
                {
                    std::vector<CefString> keys;
                    if (obj->GetKeys(keys) && keys.size() > 0)
                    {
                        auto result = CefDictionaryValue::Create();
                        for (int i = 0; i < keys.size(); i++)
                        {
                            auto p_keyStr = StringUtils::ToClr(keys[i].ToString());
                            if ((obj->HasValue(keys[i])) && (!p_keyStr->StartsWith("__")))
                            {
                                SerializeV8Object(obj->GetValue(keys[i]), result, keys[i], callbackRegistry, seen);
                            }
                        }
                        list->SetDictionary(index, result);
                    }
                }
                else
                {
                    list->SetNull(index);
                }
                seen.pop_back();
            }

            template<typename TList, typename TIndex>
            void SerializeV8Object(CefRefPtr<CefV8Value> obj, CefRefPtr<TList> list, TIndex index, JavascriptCallbackRegistry^ callbackRegistry)
            {
                try
                {
                    value_deque seen;
                    SerializeV8Object(obj, list, index, callbackRegistry, seen);
                }
                catch (const exception&)
                {
                    list->SetNull(index);
                }
            }

            template<typename TList, typename TIndex>
            CefRefPtr<CefV8Value> DeserializeV8Object(CefRefPtr<TList> list, TIndex index)
            {
                auto type = list->GetType(index);
                auto result = CefV8Value::CreateNull();

                if (type == VTYPE_BOOL)
                    result = CefV8Value::CreateBool(list->GetBool(index));
                else if (type == VTYPE_INT)
                    result = CefV8Value::CreateInt(list->GetInt(index));
                else if (type == VTYPE_DOUBLE)
                    result = CefV8Value::CreateDouble(list->GetDouble(index));
                else if (type == VTYPE_STRING)
                    result = CefV8Value::CreateString(list->GetString(index));
                else if (IsCefTime(list, index))
                    result = CefV8Value::CreateDate(GetCefTime(list, index));
                else if (type == VTYPE_LIST)
                {
                    auto subList = list->GetList(index);
                    auto size = subList->GetSize();
                    result = CefV8Value::CreateArray(size);
                    for (auto i = 0; i < size; i++)
                    {
                        result->SetValue(i, DeserializeV8Object(subList, i));
                    }
                }
                else if (type == VTYPE_DICTIONARY)
                {
                    auto subDict = list->GetDictionary(index);
                    auto size = subDict->GetSize();
                    std::vector<CefString> keys;
                    subDict->GetKeys(keys);
                    result = CefV8Value::CreateArray(size);
                    for (auto i = 0; i < size; i++)
                    {
                        result->SetValue(keys[i], DeserializeV8Object(subDict, keys[i]), V8_PROPERTY_ATTRIBUTE_NONE);
                    }
                }

                return result;
            }

            template void SerializeV8Object(CefRefPtr<CefV8Value> value, CefRefPtr<CefListValue> list, int index, JavascriptCallbackRegistry^ callbackRegistry);
            template void SerializeV8Object(CefRefPtr<CefV8Value> value, CefRefPtr<CefDictionaryValue> list, CefString index, JavascriptCallbackRegistry^ callbackRegistry);
            template void SerializeV8Object(CefRefPtr<CefV8Value> obj, CefRefPtr<CefListValue> list, int index, JavascriptCallbackRegistry^ callbackRegistry, value_deque &visited);
            template void SerializeV8Object(CefRefPtr<CefV8Value> obj, CefRefPtr<CefDictionaryValue> list, CefString index, JavascriptCallbackRegistry^ callbackRegistry, value_deque &visited);
            template CefRefPtr<CefV8Value> DeserializeV8Object(CefRefPtr<CefListValue> list, int index);
            template CefRefPtr<CefV8Value> DeserializeV8Object(CefRefPtr<CefDictionaryValue> list, CefString index);
        }
    }
}