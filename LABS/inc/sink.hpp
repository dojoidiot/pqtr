#pragma once

namespace pqtr
{

    using Bits = char *;

    class Sink
    {
    public:
        enum From
        {
            HEAD,
            TAIL
        };

        explicit Sink(int buffSize = 256)
            : headBuff_(nullptr),
              tailBuff_(nullptr),
              headSlot_(0),
              tailSlot_(0),
              buffSize_(buffSize > 0 ? buffSize : 256),
              fullSize_(0),
              buffCount_(0)
        {
            init();
        }

        ~Sink()
        {
            if (tailBuff_)
            {
                Buff *current = tailBuff_->next; // Start at what should be the head
                tailBuff_->next = nullptr;       // Break the cycle to prevent infinite loop
                while (current)
                {
                    Buff *to_delete = current;
                    current = current->next;
                    delete to_delete;
                }
            }
        }

        void push(const Bits item)
        {
            if (full())
            {
                grow();
            }

            tailBuff_->buff[tailSlot_] = item;
            tailSlot_++;
            fullSize_++;

            if (tailSlot_ == buffSize_)
            {
                tailSlot_ = 0;
                tailBuff_ = tailBuff_->next;
            }
        }

        // Take whatever bits are at the from position in the sink.
        bool take(Bits &item, From from)
        {
            if (none())
            {
                return false;
            }

            if (from == HEAD)
            {
                item = headBuff_->buff[headSlot_];
                headSlot_++;
                fullSize_--;

                if (headSlot_ == buffSize_)
                {
                    headSlot_ = 0;
                    headBuff_ = headBuff_->next;
                }
            }
            else // from == TAIL
            {
                if (tailSlot_ == 0)
                {
                    Buff *prev = tailBuff_;
                    while (prev->next != tailBuff_)
                    {
                        prev = prev->next;
                    }
                    tailBuff_ = prev;
                    tailSlot_ = buffSize_ - 1;
                }
                else
                {
                    tailSlot_--;
                }

                item = tailBuff_->buff[tailSlot_];
                fullSize_--;
            }
            return true;
        }

        // Take up to 'size' bytes from the head
        // Allocates a new buffer and copies bytes from the sink
        // Returns the actual number of bytes taken (may be less than size if sink empties)
        // Returns 0 if sink is empty
        // Caller is responsible for deleting the returned buffer with delete[]
        int take(Bits &item, int size)
        {
            if (none() || size <= 0)
            {
                item = nullptr;
                return 0;
            }

            // Determine how many bytes we can actually take
            int bytesToTake = (fullSize_ < size) ? fullSize_ : size;

            // Allocate a new buffer to hold the requested bytes
            item = new char[bytesToTake];

            int taken = 0;
            while (taken < bytesToTake && !none())
            {
                // Each element in buff is a char*, we need to dereference it
                Bits currentPtr = headBuff_->buff[headSlot_];
                item[taken] = *currentPtr;

                headSlot_++;
                fullSize_--;
                taken++;

                if (headSlot_ == buffSize_)
                {
                    headSlot_ = 0;
                    headBuff_ = headBuff_->next;
                }
            }

            return taken;
        }

        int size() const
        {
            return fullSize_;
        }

        // Tidy up the sink by marking all data as taken while preserving allocated memory
        void tidy()
        {
            // Reset to initial state with first buffer
            if (tailBuff_)
            {
                // Find the first buffer in the circular list
                Buff *firstBuff = headBuff_;
                while (firstBuff->next != headBuff_)
                {
                    firstBuff = firstBuff->next;
                }
                // Actually, headBuff_ should already point to the logical first buffer
                // Just reset both pointers to headBuff_ (or any buffer in the ring)
                headBuff_ = tailBuff_;
                tailBuff_ = headBuff_;
            }

            // Reset slots and size
            headSlot_ = 0;
            tailSlot_ = 0;
            fullSize_ = 0;
            // Note: buffCount_ is preserved to keep allocated memory
        }

        typedef void (*Take)(Bits item);
        // take the whole sink
        void take(Take call)
        {
            Bits item;
            while (this->take(item, From::HEAD))
            {
                if (call)
                {
                    call(item);
                }
            }
        }

        Sink(const Sink &) = delete;
        Sink &operator=(const Sink &) = delete;

    private:
        struct Buff
        {
            Bits *const buff;
            Buff *next;

            explicit Buff(int size) : buff(new Bits[size]), next(nullptr) {}

            ~Buff()
            {
                delete[] buff;
            }

            Buff(const Buff &) = delete;
            Buff &operator=(const Buff &) = delete;
        };
        void init()
        {
            buffCount_ = 1;
            Buff *firstBuff = new Buff(buffSize_);
            firstBuff->next = firstBuff; // Circular reference
            headBuff_ = firstBuff;
            tailBuff_ = firstBuff;
        }

        void grow()
        {
            // Inserts a new buff immediately after the current tailBuff
            Buff *newBuff = new Buff(buffSize_);
            newBuff->next = tailBuff_->next;
            tailBuff_->next = newBuff;
            buffCount_++;
        }

        bool full() const
        {
            return fullSize_ == buffCount_ * buffSize_;
        }

        bool none() const
        {
            return fullSize_ == 0;
        }
        Buff *headBuff_;
        Buff *tailBuff_;
        int headSlot_;
        int tailSlot_;
        const int buffSize_;
        int fullSize_;
        int buffCount_;
    };

} // namespace pqtr
