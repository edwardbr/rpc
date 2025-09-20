// Tests derived from the shared_ptr conformance suites in the Microsoft STL (stl/inc/memory)
// and adapted for the rpc::shared_ptr implementation (unique_ptr coverage removed).

#include <gtest/gtest.h>


#include <functional>
#include <memory>

#include <rpc/rpc.h>

#include "common/foo_impl.h"

namespace
{
    using Baz = marshalled_tests::baz;

    struct TestObject : public Baz
    {
        int payload = 42;
    };

    struct CountingDeleter
    {
        bool* was_deleted;

        void operator()(TestObject* ptr) const noexcept
        {
            if (was_deleted)
                *was_deleted = true;
            delete ptr;
        }
    };

    struct SharedBaz : public Baz, public rpc::enable_shared_from_this<SharedBaz>
    {
        int payload = 99;
    };

    struct BaseBaz : public Baz
    {
        virtual ~BaseBaz() = default;
    };

    struct DerivedBaz : public BaseBaz, public rpc::enable_shared_from_this<DerivedBaz>
    {
        int value = 7;
    };
} // namespace

TEST(SharedPtrBasic, DefaultAndRawPointerConstruction)
{
    rpc::shared_ptr<TestObject> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.use_count(), 0);
    EXPECT_FALSE(empty.unique());

    bool deleter_called = false;
    {
        rpc::shared_ptr<TestObject> sp(new TestObject);
        EXPECT_TRUE(sp);
        EXPECT_EQ(sp.use_count(), 1);
        EXPECT_TRUE(sp.unique());

        {
            rpc::shared_ptr<TestObject> sp_copy = sp;
            EXPECT_EQ(sp.use_count(), 2);
            EXPECT_FALSE(sp.unique());
        }

        EXPECT_EQ(sp.use_count(), 1);

        struct FlaggingDeleter
        {
            bool* flag;
            void operator()(TestObject* ptr) const noexcept
            {
                *flag = true;
                delete ptr;
            }
        };

        rpc::shared_ptr<TestObject> with_deleter(new TestObject, FlaggingDeleter{&deleter_called});
        EXPECT_EQ(with_deleter.use_count(), 1);
    }

    EXPECT_TRUE(deleter_called);
}

TEST(SharedPtrCustomDeleter, StoresAndInvokesDeleter)
{
    bool deleted = false;

    rpc::shared_ptr<TestObject> ptr(new TestObject, CountingDeleter{&deleted});
    auto* stored = std::get_deleter<CountingDeleter>(ptr);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->was_deleted, &deleted);

    ptr.reset();
    EXPECT_TRUE(deleted);
}

TEST(SharedPtrAliasing, SharesControlBlock)
{
    rpc::shared_ptr<TestObject> owner(new TestObject);
    TestObject* raw = owner.get();

    rpc::shared_ptr<TestObject> alias(owner, raw);
    EXPECT_EQ(owner.use_count(), 2);
    EXPECT_EQ(alias.use_count(), 2);
    EXPECT_EQ(alias.get(), raw);

    owner.reset();
    EXPECT_TRUE(alias);
    EXPECT_EQ(alias.get(), raw);
}

TEST(SharedPtrOwnerBefore, EstablishesStrictWeakOrdering)
{
    rpc::shared_ptr<TestObject> first(new TestObject);
    rpc::shared_ptr<TestObject> second(new TestObject);

    const bool first_before_second = first.owner_before(second);
    const bool second_before_first = second.owner_before(first);

    EXPECT_NE(first_before_second, second_before_first);

    rpc::shared_ptr<TestObject> alias(first, first.get());
    EXPECT_FALSE(first.owner_before(alias));
    EXPECT_FALSE(alias.owner_before(first));
}

TEST(WeakPtrOwnerBefore, MirrorsSharedPtrOrdering)
{
    rpc::shared_ptr<TestObject> first(new TestObject);
    rpc::shared_ptr<TestObject> second(new TestObject);

    rpc::weak_ptr<TestObject> weak_first(first);
    rpc::weak_ptr<TestObject> weak_second(second);

    EXPECT_NE(weak_first.owner_before(weak_second), weak_second.owner_before(weak_first));
    EXPECT_FALSE(weak_first.owner_before(first));
    EXPECT_FALSE(first.owner_before(weak_first));
}

TEST(SharedPtrEnableSharedFromThis, MakeSharedInitialisesWeakThis)
{
    auto sample = rpc::make_shared<SharedBaz>();
    EXPECT_EQ(sample->payload, 99);

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
    rpc::shared_ptr<BaseBaz> base_ptr(new DerivedBaz);
    auto derived_ptr = rpc::static_pointer_cast<DerivedBaz>(base_ptr);

    auto again = derived_ptr->shared_from_this();
    EXPECT_EQ(again.get(), derived_ptr.get());
    EXPECT_EQ(derived_ptr.use_count(), again.use_count());
}

TEST(WeakPtrLock, IncrementsAndRestoresUseCount)
{
    rpc::shared_ptr<TestObject> shared(new TestObject);
    rpc::weak_ptr<TestObject> weak(shared);

    EXPECT_EQ(shared.use_count(), 1);
    {
        auto locked = weak.lock();
        EXPECT_TRUE(locked);
        EXPECT_EQ(shared.use_count(), 2);
        EXPECT_EQ(locked.get(), shared.get());
    }
    EXPECT_EQ(shared.use_count(), 1);
}

TEST(SharedPtrHash, MatchesRawPointerHash)
{
    rpc::shared_ptr<TestObject> ptr(new TestObject);

    std::hash<rpc::shared_ptr<TestObject>> hash_shared;
    std::hash<TestObject*> hash_raw;

    EXPECT_EQ(hash_shared(ptr), hash_raw(ptr.get()));

    rpc::shared_ptr<TestObject> alias(ptr, ptr.get());
    EXPECT_EQ(hash_shared(alias), hash_shared(ptr));
}

TEST(SharedPtrUnique, ReflectsExclusiveOwnership)
{
    rpc::shared_ptr<TestObject> ptr(new TestObject);
    EXPECT_TRUE(ptr.unique());
    auto other = ptr;
    EXPECT_FALSE(ptr.unique());
    other.reset();
    EXPECT_TRUE(ptr.unique());
}
