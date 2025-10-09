#include "tekki/backend/chunky_list.h"
#include <memory>
#include <vector>
#include <stdexcept>
#include <utility>

namespace tekki::backend {

template<typename T>
const T& TempList<T>::Add(const T& item) {
    if (m_inner->Payload.size() < m_inner->Payload.capacity()) {
        m_inner->Payload.push_back(item);
        return m_inner->Payload.back();
    } else {
        auto newInner = std::make_unique<TempListInner<T>>();
        newInner->Payload.push_back(item);
        
        auto newList = std::make_shared<TempList<T>>();
        newList->m_inner = std::move(newInner);
        
        auto oldInner = std::move(m_inner);
        m_inner = std::make_unique<TempListInner<T>>();
        m_inner->Next = newList;
        m_inner->Payload = std::move(oldInner->Payload);
        
        return m_inner->Payload[0];
    }
}

template<typename T>
const T& TempList<T>::Add(T&& item) {
    if (m_inner->Payload.size() < m_inner->Payload.capacity()) {
        m_inner->Payload.push_back(std::move(item));
        return m_inner->Payload.back();
    } else {
        auto newInner = std::make_unique<TempListInner<T>>();
        newInner->Payload.push_back(std::move(item));
        
        auto newList = std::make_shared<TempList<T>>();
        newList->m_inner = std::move(newInner);
        
        auto oldInner = std::move(m_inner);
        m_inner = std::make_unique<TempListInner<T>>();
        m_inner->Next = newList;
        m_inner->Payload = std::move(oldInner->Payload);
        
        return m_inner->Payload[0];
    }
}

} // namespace tekki::backend