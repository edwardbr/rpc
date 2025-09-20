// Tests derived from the shared_ptr conformance suites in the Microsoft STL (stl/inc/memory)
// and adapted for the rpc::shared_ptr implementation (unique_ptr coverage removed).

#include <gtest/gtest.h>

#include <functional>
#include <memory>

#include <rpc/rpc.h>

namespace
{
    struct CountingDeleter
    {
        int* value;
        std::shared_ptr<int> call_counter;

        void operator()(int* ptr) noexcept
        {
            if (call_counter)
                ++(*call_counter);
            if (value)
                *value = 0;
            delete ptr;
        }
    };

    struct Sample : public rpc::enable_shared_from_this<Sample>
    {
        int payload = 42;
    };

    struct Base
    {
        virtual ~Base() = default;
    };

    struct Derived : public Base, public rpc::enable_shared_from_this<Derived>
    {
        int value = 99;
    };
} // namespace

TEST(SharedPtrBasic, DefaultAndRawPointerConstruction)
{
    rpc::shared_ptr<int> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.use_count(), 0);
    EXPECT_FALSE(empty.unique());

    bool deleter_called = false;
    {
        rpc::shared_ptr<int> sp(new int(7));
        EXPECT_TRUE(sp);
        EXPECT_EQ(*sp, 7);
        EXPECT_EQ(sp.use_count(), 1);
        EXPECT_TRUE(sp.unique());

        {
            rpc::shared_ptr<int> sp_copy = sp;
            EXPECT_EQ(sp.use_count(), 2);
            EXPECT_FALSE(sp.unique());
        }

        EXPECT_EQ(sp.use_count(), 1);

        struct FlaggingDeleter
        {
            bool* flag;
            void operator()(int* ptr) const noexcept
            {
                *flag = true;
                delete ptr;
            }
        };

        rpc::shared_ptr<int> with_deleter(new int(3), FlaggingDeleter{&deleter_called});
        EXPECT_EQ(*with_deleter, 3);
        EXPECT_EQ(with_deleter.use_count(), 1);
    }

    EXPECT_TRUE(deleter_called);
}

TEST(SharedPtrCustomDeleter, StoresAndInvokesDeleter)
{
    int sentinel = 5;
    auto counter = std::make_shared<int>(0);

    rpc::shared_ptr<int> ptr(new int(11), CountingDeleter{&sentinel, counter});
    auto* stored = std::get_deleter<CountingDeleter>(ptr);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->value, &sentinel);
    ASSERT_NE(stored->call_counter, nullptr);
    EXPECT_EQ(*stored->call_counter, 0);
    EXPECT_EQ(stored->call_counter.get(), counter.get());

    ptr.reset();
    EXPECT_EQ(sentinel, 0);
    EXPECT_EQ(*counter, 1);
}

TEST(SharedPtrAliasing, SharesControlBlock)
{
    rpc::shared_ptr<int> owner(new int(21));
    int* raw = owner.get();

    rpc::shared_ptr<int> alias(owner, raw);
    EXPECT_EQ(owner.use_count(), 2);
    EXPECT_EQ(alias.use_count(), 2);
    EXPECT_EQ(alias.get(), raw);

    owner.reset();
    EXPECT_TRUE(alias);
    EXPECT_EQ(*alias, 21);
}

TEST(SharedPtrOwnerBefore, EstablishesStrictWeakOrdering)
{
    rpc::shared_ptr<int> first(new int(1));
    rpc::shared_ptr<int> second(new int(2));

    const bool first_before_second = first.owner_before(second);
    const bool second_before_first = second.owner_before(first);

    EXPECT_NE(first_before_second, second_before_first);

    rpc::shared_ptr<int> alias(first, first.get());
    EXPECT_FALSE(first.owner_before(alias));
    EXPECT_FALSE(alias.owner_before(first));
}

TEST(WeakPtrOwnerBefore, MirrorsSharedPtrOrdering)
{
    rpc::shared_ptr<int> first(new int(10));
    rpc::shared_ptr<int> second(new int(20));

    rpc::weak_ptr<int> weak_first(first);
    rpc::weak_ptr<int> weak_second(second);

    EXPECT_NE(weak_first.owner_before(weak_second), weak_second.owner_before(weak_first));
    EXPECT_FALSE(weak_first.owner_before(first));
    EXPECT_FALSE(first.owner_before(weak_first));
}

TEST(SharedPtrEnableSharedFromThis, MakeSharedInitialisesWeakThis)
{
    auto sample = rpc::make_shared<Sample>();
    EXPECT_EQ(sample->payload, 42);

    auto again = sample->shared_from_this();
    EXPECT_EQ(sample.get(), again.get());
    EXPECT_EQ(sample.use_count(), again.use_count());

    auto weak = sample->weak_from_this();
    EXPECT_FALSE(weak.expired());
    auto locked = weak.lock();
    EXPECT_EQ(locked.get(), sample.get());
    EXPECT_EQ(sample.use_count(), locked.use_count());
}

TEST(SharedPtrEnableSharedFromThis, WorksThroughBaseConstruction)
{
    rpc::shared_ptr<Base> base_ptr(new Derived);
    auto derived_ptr = rpc::static_pointer_cast<Derived>(base_ptr);

    auto again = derived_ptr->shared_from_this();
    EXPECT_EQ(again.get(), derived_ptr.get());
    EXPECT_EQ(derived_ptr.use_count(), again.use_count());
}

TEST(WeakPtrLock, IncrementsAndRestoresUseCount)
{
    rpc::shared_ptr<int> shared(new int(55));
    rpc::weak_ptr<int> weak(shared);

    EXPECT_EQ(shared.use_count(), 1);
    {
        auto locked = weak.lock();
        EXPECT_TRUE(locked);
        EXPECT_EQ(shared.use_count(), 2);
        EXPECT_EQ(*locked, 55);
    }
    EXPECT_EQ(shared.use_count(), 1);
}

TEST(SharedPtrHash, MatchesRawPointerHash)
{
    rpc::shared_ptr<int> ptr(new int(88));

    std::hash<rpc::shared_ptr<int>> hash_shared;
    std::hash<int*> hash_raw;

    EXPECT_EQ(hash_shared(ptr), hash_raw(ptr.get()));

    rpc::shared_ptr<int> alias(ptr, ptr.get());
    EXPECT_EQ(hash_shared(alias), hash_shared(ptr));
}

TEST(SharedPtrUnique, ReflectsExclusiveOwnership)
{
    rpc::shared_ptr<int> ptr(new int(5));
    EXPECT_TRUE(ptr.unique());
    auto other = ptr;
    EXPECT_FALSE(ptr.unique());
    other.reset();
    EXPECT_TRUE(ptr.unique());
}
