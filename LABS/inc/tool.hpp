#pragma once

#include <sink.hpp>
#include <string>
#include <fstream>
#include <stdexcept>

namespace pqtr
{

    // Utility functions for file I/O with Sink
    // Reads files into Sinks, writes Sinks to files
    class Tool
    {
    public:
        // Read entire file into a Sink
        // File is read in chunks (network-sized for async I/O compatibility)
        // Returns pointer to Sink, caller owns and must delete
        // Throws std::runtime_error if file cannot be opened
        static Sink* read(const std::string &filename)
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to open file: " + filename);
            }

            Sink* sink = new Sink();

            // Read file in network-sized chunks (typical async I/O size)
            const size_t chunkSize = 65536;  // 64KB

            while (!file.eof())
            {
                char* chunk = new char[chunkSize];
                file.read(chunk, chunkSize);
                std::streamsize bytesRead = file.gcount();

                if (bytesRead > 0)
                {
                    // If partial read, shrink allocation to actual size
                    if (bytesRead < (std::streamsize)chunkSize)
                    {
                        char* exact = new char[bytesRead];
                        std::memcpy(exact, chunk, bytesRead);
                        delete[] chunk;
                        chunk = exact;
                    }

                    // Push chunk to sink (sink takes ownership)
                    sink->push(chunk, bytesRead);
                }
                else
                {
                    delete[] chunk;
                }
            }

            return sink;
        }

        // Write Sink contents to a file
        // Throws std::runtime_error if file cannot be opened
        static void save(Sink &sink, const std::string &filename)
        {
            std::ofstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to open file for writing: " + filename);
            }

            // Read entire sink in chunks and write to file
            const size_t chunkSize = 65536;
            char* buffer;

            while (true)
            {
                int bytesRead = sink.take(buffer, chunkSize);
                if (bytesRead <= 0)
                {
                    break;
                }

                file.write(buffer, bytesRead);
                delete[] buffer;
            }
        }
    };

} // namespace pqtr
