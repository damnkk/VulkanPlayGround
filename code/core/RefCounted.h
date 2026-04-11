#pragma once

#include <atomic>
#include <cstdint>
#include <cassert>

namespace Play
{

/**
 * @brief 侵入式引用计数基类
 *
 * 设计要点：
 * 1. 强引用计数控制对象生命周期（归零时立即 delete 对象）
 * 2. 弱引用仅用于有效性检测，不控制生命周期
 * 3. 初始强引用为 0（创建时未被持有，RefPtr 接管时+1）
 * 4. 线程安全（使用 atomic）
 * 5. 子类实现 onDestroy() 将 Vulkan handle 提交到延迟析构队列
 */
class RefCounted
{
public:
    RefCounted() : m_strongCount(0), m_weakCount(0) {}

    // 禁止拷贝和赋值
    RefCounted(const RefCounted&) = delete;
    RefCounted& operator=(const RefCounted&) = delete;

    // 增加强引用计数
    void addRef() const
    {
        m_strongCount.fetch_add(1, std::memory_order_relaxed);
    }

    // 释放强引用计数，当计数归零时立即删除对象
    void release() const
    {
        if (m_strongCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // 强引用归零，调用子类的销毁逻辑（将 Vulkan handle 提交到延迟队列）
            const_cast<RefCounted*>(this)->onDestroy();

            // 标记对象已死亡（用于弱引用检测）
            m_strongCount.store(0, std::memory_order_release);

            // 立即删除对象本身
            delete this;
        }
    }

    // 增加弱引用计数
    void addWeakRef() const
    {
        m_weakCount.fetch_add(1, std::memory_order_relaxed);
    }

    // 释放弱引用计数（仅用于计数，不影响对象生命周期）
    void releaseWeakRef() const
    {
        m_weakCount.fetch_sub(1, std::memory_order_relaxed);
    }

    // 获取当前强引用计数（主要用于调试）
    uint32_t getRefCount() const
    {
        return m_strongCount.load(std::memory_order_relaxed);
    }

    // 检查对象是否还存活（强引用 > 0）
    bool isAlive() const
    {
        return m_strongCount.load(std::memory_order_acquire) > 0;
    }

    // 强制销毁（仅用于 VulkanDriver 清理泄露对象）
    void forceDestroy()
    {
        if (isAlive())
        {
            onDestroy();
            m_strongCount.store(0, std::memory_order_release);
        }
    }

protected:
    virtual ~RefCounted() = default;

    /**
     * @brief 当强引用归零时调用，子类实现 Vulkan handle 的延迟析构
     *
     * 典型实现：
     * - 将 VkBuffer/VkImage 等 handle 提交到 VulkanDriver 的延迟析构队列
     * - 从 VulkanDriver 注册表中移除自己
     * - 注意：此时对象本身即将被 delete，不要访问成员变量
     */
    virtual void onDestroy() = 0;

private:
    mutable std::atomic<uint32_t> m_strongCount;
    mutable std::atomic<uint32_t> m_weakCount; // 仅用于统计，不控制生命周期
};

// 前向声明
template <typename T>
class WeakRefPtr;

/**
 * @brief 强引用智能指针
 *
 * 用法：
 * - RefPtr<Buffer> buffer = new Buffer(...);
 * - buffer->someMethod();
 * - Buffer* rawPtr = buffer.get(); // 获取裸指针（不增加引用计数）
 */
template <typename T>
class RefPtr
{
public:
    RefPtr() : m_ptr(nullptr) {}

    // 从裸指针构造（接管所有权，增加引用计数）
    explicit RefPtr(T* ptr) : m_ptr(ptr)
    {
        if (m_ptr)
            m_ptr->addRef();
    }

    // 拷贝构造
    RefPtr(const RefPtr& other) : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            m_ptr->addRef();
    }

    // 移动构造
    RefPtr(RefPtr&& other) noexcept : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    // 从其他类型转换（支持派生类到基类）
    template <typename U>
    RefPtr(const RefPtr<U>& other) : m_ptr(other.get())
    {
        if (m_ptr)
            m_ptr->addRef();
    }

    ~RefPtr()
    {
        if (m_ptr)
            m_ptr->release();
    }

    // 拷贝赋值
    RefPtr& operator=(const RefPtr& other)
    {
        if (this != &other)
        {
            if (other.m_ptr)
                other.m_ptr->addRef();
            if (m_ptr)
                m_ptr->release();
            m_ptr = other.m_ptr;
        }
        return *this;
    }

    // 移动赋值
    RefPtr& operator=(RefPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (m_ptr)
                m_ptr->release();
            m_ptr       = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    // 从裸指针赋值
    RefPtr& operator=(T* ptr)
    {
        if (m_ptr != ptr)
        {
            if (ptr)
                ptr->addRef();
            if (m_ptr)
                m_ptr->release();
            m_ptr = ptr;
        }
        return *this;
    }

    // 解引用
    T& operator*() const
    {
        assert(m_ptr != nullptr);
        return *m_ptr;
    }

    T* operator->() const
    {
        assert(m_ptr != nullptr);
        return m_ptr;
    }

    // 获取裸指针
    T* get() const
    {
        return m_ptr;
    }

    // 布尔转换
    explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    // 重置指针
    void reset()
    {
        if (m_ptr)
        {
            m_ptr->release();
            m_ptr = nullptr;
        }
    }

    // 比较运算符
    bool operator==(const RefPtr& other) const { return m_ptr == other.m_ptr; }
    bool operator!=(const RefPtr& other) const { return m_ptr != other.m_ptr; }
    bool operator==(const T* ptr) const { return m_ptr == ptr; }
    bool operator!=(const T* ptr) const { return m_ptr != ptr; }

private:
    template <typename U>
    friend class RefPtr;
    template <typename U>
    friend class WeakRefPtr;

    T* m_ptr;
};

/**
 * @brief 弱引用智能指针
 *
 * 用途：
 * - VulkanDriver 注册表使用弱引用持有对象
 * - 不影响对象生命周期
 * - 可以检测对象是否已销毁
 */
template <typename T>
class WeakRefPtr
{
public:
    WeakRefPtr() : m_ptr(nullptr) {}

    // 从强引用构造
    WeakRefPtr(const RefPtr<T>& ref) : m_ptr(ref.get())
    {
        if (m_ptr)
            m_ptr->addWeakRef();
    }

    // 拷贝构造
    WeakRefPtr(const WeakRefPtr& other) : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            m_ptr->addWeakRef();
    }

    // 移动构造
    WeakRefPtr(WeakRefPtr&& other) noexcept : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    ~WeakRefPtr()
    {
        if (m_ptr)
            m_ptr->releaseWeakRef();
    }

    // 拷贝赋值
    WeakRefPtr& operator=(const WeakRefPtr& other)
    {
        if (this != &other)
        {
            if (other.m_ptr)
                other.m_ptr->addWeakRef();
            if (m_ptr)
                m_ptr->releaseWeakRef();
            m_ptr = other.m_ptr;
        }
        return *this;
    }

    // 移动赋值
    WeakRefPtr& operator=(WeakRefPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (m_ptr)
                m_ptr->releaseWeakRef();
            m_ptr       = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    // 尝试提升为强引用（如果对象还存活）
    RefPtr<T> lock() const
    {
        if (m_ptr && m_ptr->isAlive())
        {
            m_ptr->addRef();
            return RefPtr<T>(m_ptr);
        }
        return RefPtr<T>(nullptr);
    }

    // 检查对象是否已销毁
    bool expired() const
    {
        return !m_ptr || !m_ptr->isAlive();
    }

    // 获取裸指针（危险：不保证对象存活）
    T* unsafeGet() const
    {
        return m_ptr;
    }

    void reset()
    {
        if (m_ptr)
        {
            m_ptr->releaseWeakRef();
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};

} // namespace Play
