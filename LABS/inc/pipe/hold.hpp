#pragma once

namespace pqtr
{

    /** Hold is used for smart raw pointers.  Factory methods must make and return Holds on objects as the objects have to
     * be made on the heap not the program stack like references are.  Basically we are using all pointers not references.
     */
    template <typename T>
    class Hold
    {
    private:
        T *_ptr;

    public:
        // Constructor: Takes ownership of the raw pointer.
        explicit Hold(T *ptr = nullptr) : _ptr(ptr) {}

        // Destructor: Automatically deletes the object when the 'Hold' goes out of scope.
        ~Hold()
        {
            delete _ptr;
        }

        // --- Rule of Five: Handling Ownership ---
        // 1. Delete the copy constructor.
        Hold(const Hold &other) = delete;

        // 2. Delete the copy assignment operator.
        Hold &operator=(const Hold &other) = delete;

        // 3. Move constructor: Transfers ownership from 'other' to this 'Hold'.
        Hold(Hold &&other) noexcept : _ptr(other._ptr)
        {
            other._ptr = nullptr;
        }

        // 4. Move assignment operator: Transfers ownership.
        Hold &operator=(Hold &&other) noexcept
        {
            if (this != &other)
            {
                delete _ptr;          // Delete the object we are currently holding.
                _ptr = other._ptr;    // Steal the pointer from the other object.
                other._ptr = nullptr; // Set the other object's pointer to null.
            }
            return *this;
        }

        // --- Behaving Like a Pointer ---
        // Overload the dereference operator (*) to access the object.
        T &operator*() const
        {
            return *_ptr;
        }

        // Overload the arrow operator (->) to access the object's members.
        T *operator->() const
        {
            return _ptr;
        }

        // Check if holding a valid pointer
        explicit operator bool() const
        {
            return _ptr != nullptr;
        }
    };

} // namespace pqtr
