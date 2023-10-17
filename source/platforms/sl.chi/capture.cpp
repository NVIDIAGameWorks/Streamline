#include "capture.h"

#ifdef SL_CAPTURE

#include "source/core/sl.log/log.h"
#include "source/platforms/sl.chi/compute.h"

#include <time.h> 
#include <fstream>
#include <filesystem>
#include <future>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <map>
#include <thread>
#pragma warning( disable : 4996)

namespace sl
{
    namespace chi
    {

        struct Capture : public ICapture
        {
            ICompute* compute;
            int maxCaptureIndex = 100;
            int captureIndex = INT_MIN; // How many frames have been captures so far.
            std::mutex captureStreamMutex; // Mutex for threading consistency.
            std::atomic<bool> isCapturing = false; // tell if we are capturing.
            std::chrono::steady_clock::time_point startTime; // start time of the capture session.
            std::vector<std::pair<char*, uint64_t>> pendingDumps; // Vector of contents and length dumps that we have accumulated and are waiting to dump to the file.
            std::string fullPath = ""; // Filepath to use when opening a file.
            std::thread dumpthread;
            std::mutex mtx;

            std::map<BufferType, ResourceReadbackQueue> m_readbackMap; //Must be destroyed in the API
            std::vector<std::future<bool>> m_readbackThreads;

            /// <summary>
            /// Deallocate
            /// </summary>
            ~Capture();

            /// <summary>
            /// Initialize
            /// </summary>
            virtual void init(ICompute* compute_ptr) override final;

            /// <summary>
            /// Sets the maxCaptureIndex
            /// </summary>
            virtual void setMaxCaptureIndex(int maxCaptureIndex_in) override final;

            /// <summary>
            /// Dumps the contents of a resource using api-specific terminology. This should call addPendingResourceBinary at some point.
            /// </summary>
            virtual ComputeStatus dumpResource(int id, BufferType type, Extent& extent, CommandList cmdList, Resource src) override final;

            /// <summary>
            /// Resource description and pixel data to the binary data to the pendingDumps.
            /// </summary>
            virtual ComputeStatus appendResourceDump(int id, BufferType type, Extent extent, ResourceDescription srcDesc, char* pixels, uint64_t bytes) override final;

            /// <summary>
            /// Adds Global Constants to the pending Dumps. 
            /// </summary>
            virtual ComputeStatus appendGlobalConstantDump(int id, double time, Constants const* ptrConsts) override final;

            /// <summary>
            /// Adds Feature Specific Constants to the pending Dumps. 
            /// </summary>
            virtual ComputeStatus appendFeatureStructureDump(int id, int counter, void const* ptrConsts, int sizeConsts) override final;

            /// <summary>
            /// Start recording dumps: establishes file name with date/time.
            /// </summary>
            virtual ComputeStatus startRecording(std::string plugin, std::string path = "./") override final;

            /// <summary>
            /// add arbitrary content to the pending Dumps.
            /// </summary>
            virtual ComputeStatus addToPending(char* dump, uint64_t size) override final;

            /// <summary>
            /// Dumps the pendingDumps into a file all at once and ends the capture.
            /// </summary>
            virtual ComputeStatus dumpPending() override final;

            /// <summary>
            /// Return time since startRecording was called. Helpful for tracking frame intervals.
            /// </summary>
            virtual double getTimeSinceStart() override final;

            /// <summary>
            /// Get Date/Time
            /// </summary>
            virtual std::string getDateTime() override final;

            /// <summary>
            /// Increment Capture index so we know how many frames we have recorded thus far.
            /// </summary>
            virtual void incrementCaptureIndex() override final;

            /// <summary>
            /// Get the CaptureIndex.
            /// </summary>
            virtual int getCaptureIndex() override final;

            /// <summary>
            /// Tell if we have reached the max number of frames we wanted to capture.
            /// </summary>
            virtual bool getIndexHasReachedMaxCapatureIndex() override final;

            /// <summary>
            /// Tell if we are currently capturing.
            /// </summary>
            virtual bool getIsCapturing() override final;
        };

        Capture CaptureSystem = Capture{};

        ICapture* getCapture() {
            return &CaptureSystem;
        }

        ComputeStatus cleanResources(ICompute* compute, std::map<BufferType, ResourceReadbackQueue>* readbackMap) {
            for (auto& rb : *readbackMap)
            {
                CHI_CHECK(compute->destroyResource(rb.second.target));
                for (int i = 0; i < SL_DUMP_QUEUE_SIZE; i++)
                {
                    CHI_CHECK(compute->destroyResource(rb.second.readback[i]));
                }
            }
            readbackMap->clear();
            return ComputeStatus::eOk;
        }

        ComputeStatus dump_threadFunction(
            ICompute* compute,
            std::atomic_bool* isCapturing,
            int captureIndex,
            std::string fullPath, 
            std::mutex* captureStreamMutex,
            std::vector<std::pair<char*, uint64_t>>* pendingDumps,
            std::vector<std::future<bool>>* m_readbackThreads,
            std::map<BufferType, ResourceReadbackQueue>* readbackMap)
        {
            
            // Stop the capturing
            {
                std::scoped_lock lock(*captureStreamMutex);

                if (!isCapturing->load()) {
                    SL_LOG_WARN("capture.cpp - Capture must be in progress to dump");
                    return ComputeStatus::eError;
                }
            }

            // Wait for all the threads to finish
            for (int i = 0; i < m_readbackThreads->size(); i++) m_readbackThreads->at(i).wait();

            // Now we lock the capture and we write to file.
            std::scoped_lock lock(*captureStreamMutex);

            isCapturing->store(false);

            /// Capture
            auto captureStream = std::ofstream(fullPath.c_str(), std::ios::binary);

            if (!captureStream.is_open())
            {
                SL_LOG_WARN("Capture: Failed to open filestream.");
                captureStream.close();
                return ComputeStatus::eError;
            }

            for (auto i : (*pendingDumps)) captureStream.write(i.first, i.second);

            if (!captureStream.good())
            {
                SL_LOG_WARN("Capture: Error while writing to filestream.");
                captureStream.close();
                return ComputeStatus::eError;
            }
            captureStream.close();

            // Clean up 
            m_readbackThreads->clear();
            for (auto i : (*pendingDumps)) delete[] i.first;
            pendingDumps->clear();
            cleanResources(compute, readbackMap);

            SL_LOG_INFO("Capture: Dump finished successfully.");

            return ComputeStatus::eOk;
        }

        Capture::~Capture() {
            if (dumpthread.joinable()) dumpthread.join();
        }


        void Capture::init(ICompute* compute_ptr) {
            compute = compute_ptr;
        }

        void Capture::setMaxCaptureIndex(int maxCaptureIndex_in) {
            maxCaptureIndex = maxCaptureIndex;
        }

        ComputeStatus Capture::dumpResource(int id, BufferType type, Extent& extent, CommandList cmdList, Resource src)
        {
            compute->bindSharedState(cmdList, 0);

            ResourceDescription srcDesc;
            CHI_CHECK(compute->getResourceDescription(src, srcDesc));

            // Find the ResourceReadbackQueue for a resource, if not there then create it
            ResourceReadbackQueue* rrq = {};
            {
                std::scoped_lock<std::mutex> lock(mtx);
                auto it = m_readbackMap.find(type);
                if (it == m_readbackMap.end()) {
                    m_readbackMap[type] = {};
                    rrq = &m_readbackMap[type];
                }
                else rrq = &(*it).second;
            }

            // Get the byte size of the resource
            size_t bpp;
            auto format = srcDesc.format;
            if (format == eFormatINVALID && srcDesc.nativeFormat != NativeFormatUnknown)
            {
                compute->getFormat(srcDesc.nativeFormat, format);
                if (format == eFormatINVALID)
                {
                    SL_LOG_WARN("Don't know the size for resource 0x%llx format %u native %u", src, srcDesc.format, srcDesc.nativeFormat);
                }
            }

            compute->getBytesPerPixel(format, bpp);
            uint64_t predictedbytes = bpp * srcDesc.width * srcDesc.height;
            uint64_t rowSizeInBytes = bpp * srcDesc.width;

            ResourceFootprint footprint;
            compute->getResourceFootprint(src, footprint);
            uint64_t bytes = footprint.totalBytes;

            // if the readback has a buffer then check its readback states.
            if (rrq->readback[rrq->index])
            {
                // Shortcut incase id is not yet caught up to lag
                if (id >= 0) {

                    // If there is already a buffer
                    // Create a map between the resource and a data pointer
                    // And read the data
                    {
                        Resource res = rrq->readback[rrq->index];
                        void* data;
                        compute->mapResource(cmdList, res, data, 0, 0, bytes);
                        if (!data) SL_LOG_WARN("Capture: Failed to map readback resource.");
                        else {
                            char* pixels = new char[predictedbytes];
                            for (uint64_t y = 0; y < srcDesc.height; y++)
                            {
                                void* gpu_ptr = (char*)data + y * footprint.rowPitch;
                                void* cpu_ptr = (char*)pixels + y * rowSizeInBytes;
                                memcpy(cpu_ptr, gpu_ptr, rowSizeInBytes);
                            }
                            compute->unmapResource(cmdList, res, 0);
                            // Launch a thread to add it to queue
                            m_readbackThreads.push_back(std::async(std::launch::async, [this, id, type, extent, srcDesc, pixels, predictedbytes]()->bool {
                                return this->appendResourceDump(id, type, extent, srcDesc, pixels, predictedbytes);
                                }));
                        }
                    }
                }
            }
            // if it does not have a buffer
            else
            {
                // create a readback buffer for CPU access
                ResourceDescription desc((uint32_t) bytes, 1, chi::eFormatINVALID, chi::eHeapTypeReadback, chi::ResourceState::eCopyDestination);
                CHI_CHECK(compute->createBuffer(desc, rrq->readback[rrq->index], (std::string("chi.capture.") + std::to_string((size_t)src) + "." + std::to_string(rrq->index)).c_str()));
            }

            // Once we are done creating/using the resource
            // Transition an and copy the buffer to it
            {
                extra::ScopedTasks revTransitions;
                chi::ResourceTransition transitions[] =
                {
                    {src, chi::ResourceState::eCopySource, srcDesc.state},
                };
                CHI_CHECK(compute->transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions));

                compute->copyDeviceTextureToDeviceBuffer(cmdList, src, rrq->readback[rrq->index]);
                
            }

            // increment the index
            rrq->index = (rrq->index + 1) % SL_DUMP_QUEUE_SIZE;

            return ComputeStatus::eOk;
        }


        ComputeStatus Capture::appendResourceDump(int id, BufferType type, Extent extent, ResourceDescription srcDesc, char* pixels, uint64_t bytes) {
            /// <summary>
            /// Adds resource data to the dump
            /// </summary>

            // The capture runs with a delay of SL_DUMP_QUEUE_SIZE. So we need to pre-empt it.
            if (id < 0) { return ComputeStatus::eOk; }

            uint64_t totalSize = SL_DUMP_SIZE_OF_LABELS + sizeof(int) + sizeof(BufferType) + sizeof(Extent) + sizeof(ResourceDescription) + sizeof(uint64_t) + bytes;

            char* data = new char[totalSize];

            totalSize = 0;

            memcpy(data + totalSize, resourceLabel, SL_DUMP_SIZE_OF_LABELS);
            totalSize += SL_DUMP_SIZE_OF_LABELS;

            memcpy(data + totalSize, &id, sizeof(int));
            totalSize += sizeof(int);

            memcpy(data + totalSize, &type, sizeof(BufferType));
            totalSize += sizeof(BufferType);

            memcpy(data + totalSize, &extent, sizeof(Extent));
            totalSize += sizeof(Extent);

            memcpy(data + totalSize, &srcDesc, sizeof(ResourceDescription));
            totalSize += sizeof(ResourceDescription);

            memcpy(data + totalSize, pixels, bytes);
            totalSize += bytes;
            delete[] pixels;

            ComputeStatus status = addToPending(data, totalSize);

            return status;
        }

        ComputeStatus Capture::appendGlobalConstantDump(int id, double time, Constants const* ptrConsts) {
            /// <summary>
            /// Appends the Global onstants set of constants to the list of pending dumps.
            /// </summary>

            // The capture runs with a delay of SL_DUMP_QUEUE_SIZE. So we need to pre-empt it.
            if (id < 0) { return ComputeStatus::eOk; }
            
            uint64_t totalSize = SL_DUMP_SIZE_OF_LABELS + sizeof(int) + sizeof(double) + sizeof(Constants);

            char* data = new char[totalSize];

            totalSize = 0;

            memcpy(data + totalSize, constGloLabel, SL_DUMP_SIZE_OF_LABELS);
            totalSize += SL_DUMP_SIZE_OF_LABELS;

            memcpy(data + totalSize, &id, sizeof(int));
            totalSize += sizeof(int);

            memcpy(data + totalSize, &time, sizeof(double));
            totalSize += sizeof(double);

            memcpy(data + totalSize, ptrConsts, sizeof(Constants));
            totalSize += sizeof(Constants);

            return addToPending(data, totalSize);

        }

        ComputeStatus Capture::appendFeatureStructureDump(int id, int counter, void const* ptrConsts, int sizeConsts) {
            /// <summary>
            /// Appends a Feature set of constants to the list of pending dumps.
            /// </summary>

            // The capture runs with a delay of SL_DUMP_QUEUE_SIZE. So we need to pre-empt it.
            if (id < 0) { return ComputeStatus::eOk; }

            uint64_t totalSize = SL_DUMP_SIZE_OF_LABELS + sizeof(int) + sizeof(int) + sizeConsts;

            char* data = new char[totalSize];

            totalSize = 0;

            memcpy(data + totalSize, constFeaLabel, SL_DUMP_SIZE_OF_LABELS);
            totalSize += SL_DUMP_SIZE_OF_LABELS;

            memcpy(data + totalSize, &id, sizeof(int));
            totalSize += sizeof(int);

            memcpy(data + totalSize, &counter, sizeof(int));
            totalSize += sizeof(int);

            memcpy(data + totalSize, ptrConsts, sizeConsts);
            totalSize += sizeConsts;

            return addToPending(data, totalSize);

        }

        ComputeStatus Capture::startRecording(std::string plugin, std::string path) {
            std::scoped_lock lock(captureStreamMutex);

            if (compute == nullptr) return ComputeStatus::eError;

            if (isCapturing) {
                SL_LOG_INFO("Capture: Already in progress. Please wait until finished");
                return ComputeStatus::eError;
            }

            auto dt = getDateTime();
            fullPath = path + "SLCapture_" + std::to_string(maxCaptureIndex) + "_" + plugin + "_" + dt +".sldump";
            startTime = std::chrono::high_resolution_clock::now();

            captureIndex = -SL_DUMP_QUEUE_SIZE;
            isCapturing = true;
            SL_LOG_INFO("Caputure: Start - %i frames for plugin %s", maxCaptureIndex, plugin.c_str());

            return ComputeStatus::eOk;
        }

        ComputeStatus Capture::addToPending(char* dump, uint64_t size) {
            std::scoped_lock lock(captureStreamMutex);

            if (!isCapturing) return ComputeStatus::eError;

            pendingDumps.push_back(std::make_pair(dump, size));

            return ComputeStatus::eOk;
        }

        ComputeStatus Capture::dumpPending() {

            if (!isCapturing) {
                SL_LOG_INFO("Capture: Error as trying to dump while not capturing.");
                return ComputeStatus::eError;
            }

            if (dumpthread.joinable()) dumpthread.join();
            dumpthread = std::thread(&dump_threadFunction, compute, &isCapturing, captureIndex, fullPath, &captureStreamMutex, &pendingDumps, &m_readbackThreads, &m_readbackMap);
            
            captureIndex = INT_MIN;
            return ComputeStatus::eOk;
        }

        double Capture::getTimeSinceStart() {
            return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - startTime).count();
        }

        std::string Capture::getDateTime() {
            std::time_t t0 = time(NULL);   // get time now
            std::tm* now = localtime(&t0);
            return std::to_string(now->tm_year + 1900) + "-" + std::to_string(now->tm_mon + 1) + "-" + std::to_string(now->tm_mday) + "-" + std::to_string(now->tm_hour) + "-" + std::to_string(now->tm_min) + "-" + std::to_string(now->tm_sec);
        }

        void Capture::incrementCaptureIndex() {
            captureIndex += 1;
        }

        int Capture::getCaptureIndex() {
            return captureIndex;
        }


        bool Capture::getIndexHasReachedMaxCapatureIndex() {
            // We want to make sure to go past the max frame by SL_DUMP_QUEUE_SIZE so that the threads can catch up.
            return captureIndex == maxCaptureIndex;
        }

        bool Capture::getIsCapturing() {
            return isCapturing;
        }

    }
}

#endif
