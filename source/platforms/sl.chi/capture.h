#pragma once

#include <string>

#include "include/sl.h"

#ifndef SL_PRODUCTION
#define SL_CAPTURE
#endif

#ifdef SL_CAPTURE

namespace sl
{

    constexpr char resourceLabel[] = "\nTYPE_RESOURCE     \n";
    constexpr char constGloLabel[] = "\nTYPE_CONST_GLOBAL \n";
    constexpr char constFeaLabel[] = "\nTYPE_CONST_FEATURE\n";

    constexpr int SL_DUMP_QUEUE_SIZE = 3; // Size of Capture Queue. This also defines the lag between frame copy and text copy.
    constexpr int SL_DUMP_SIZE_OF_LABELS = 20; // This is the size of all the predefined str labels when exporting the binary. The constant size makes it easier to parse.

    namespace chi
    {

        enum ComputeStatus;
        class ICompute;
        struct ResourceDescription;
        using CommandList = void*;
        using Resource = sl::Resource*;

        struct ResourceReadbackQueue
        {
            Resource target;
            Resource readback[SL_DUMP_QUEUE_SIZE];
            uint32_t index = 0;
        };

        /// <summary>
        /// This class encapsulates are the capture mechanisms.
        // </summary>
        struct ICapture {

            /// <summary>
            /// Initialize
            /// </summary>
            virtual void init(ICompute* compute_ptr) = 0;

            /// <summary>
            /// Sets the maxCaptureIndex
            /// </summary>
            virtual void setMaxCaptureIndex(int maxCaptureIndex_in) = 0;

            /// <summary>
            /// Dumps the contents of a resource using api-specific terminology. This should call addPendingResourceBinary at some point.
            /// </summary>
            virtual ComputeStatus dumpResource(int id, BufferType type, Extent& extent, CommandList cmdList, Resource src) = 0;

            /// <summary>
            /// Resource description and pixel data to the binary data to the pendingDumps.
            /// </summary>
            virtual ComputeStatus appendResourceDump(int id, BufferType type, Extent extent, ResourceDescription srcDesc, char* pixels, uint64_t bytes) = 0;

            /// <summary>
            /// Adds Global Constants to the pending Dumps. 
            /// </summary>
            virtual ComputeStatus appendGlobalConstantDump(int id, double time, Constants const* ptrConsts) = 0;

            /// <summary>
            /// Adds Feature Specific Constants to the pending Dumps. 
            /// </summary>
            virtual ComputeStatus appendFeatureStructureDump(int id, int counter, void const* ptrConsts, int sizeConsts) = 0;

            /// <summary>
            /// Start recording dumps: establishes file name with date/time.
            /// </summary>
            virtual ComputeStatus startRecording(std::string plugin, std::string path = "./") = 0;

            /// <summary>
            /// add arbitrary content to the pending Dumps.
            /// </summary>
            virtual ComputeStatus addToPending(char* dump, uint64_t size) = 0;

            /// <summary>
            /// Dumps the pendingDumps into a file all at once and ends the capture.
            /// </summary>
            virtual ComputeStatus dumpPending() = 0;

            /// <summary>
            /// Return time since startRecording was called. Helpful for tracking frame intervals.
            /// </summary>
            virtual double getTimeSinceStart() = 0;

            /// <summary>
            /// Get Date/Time
            /// </summary>
            virtual std::string getDateTime() = 0;

            /// <summary>
            /// Increment Capture index so we know how many frames we have recorded thus far.
            /// </summary>
            virtual void incrementCaptureIndex() = 0;

            /// <summary>
            /// Get the CaptureIndex.
            /// </summary>
            virtual int getCaptureIndex() = 0;

            /// <summary>
            /// Tell if we have reached the max number of frames we wanted to capture.
            /// </summary>
            virtual bool getIndexHasReachedMaxCapatureIndex() = 0;

            /// <summary>
            /// Tell if we are currently capturing.
            /// </summary>
            virtual bool getIsCapturing() = 0;
        };

        ICapture* getCapture();

    }
}

#endif
