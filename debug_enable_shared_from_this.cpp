#include <iostream>
#include <cassert>
#include <rpc/internal/remote_pointer.h>

#define TEST_STL_COMPLIANCE

using namespace std;

struct X : enable_shared_from_this<X> {
    explicit X(const int n) : m_n(n) {
        cout << "X constructed with n=" << n << ", m_n=" << m_n << endl;
        cout << "enable_shared_from_this base address: " << static_cast<enable_shared_from_this<X>*>(this) << endl;
    }
    int m_n;
};

int main() {
    cout << "Creating shared_ptr<X> with new X(11)" << endl;
    const shared_ptr<X> sp1(new X(11));

    cout << "Getting raw pointer" << endl;
    X* const raw1 = sp1.get();

    cout << "Checking m_n: " << raw1->m_n << endl;
    cout << "Expected: 11" << endl;

    if (raw1->m_n == 11) {
        cout << "SUCCESS: m_n is correct" << endl;
    } else {
        cout << "FAILED: m_n is wrong" << endl;
        return 1;
    }

    cout << "Testing shared_from_this()" << endl;
    auto shared_this = raw1->shared_from_this();
    cout << "shared_from_this() returned: " << shared_this.get() << endl;
    cout << "Original pointer: " << sp1.get() << endl;

    if (shared_this == sp1) {
        cout << "SUCCESS: shared_from_this() works" << endl;
    } else {
        cout << "FAILED: shared_from_this() doesn't match" << endl;
    }

    return 0;
}