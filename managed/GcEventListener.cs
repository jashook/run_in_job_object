////////////////////////////////////////////////////////////////////////////////
// Module: GcEventListener.cs
//
////////////////////////////////////////////////////////////////////////////////

namespace ev30
{
    using System;
    using System.Diagnostics.Tracing;
    
    sealed class GcEventListener : EventListener
    {
        // from https://docs.microsoft.com/en-us/dotnet/framework/performance/garbage-collection-etw-events
        private const int GC_KEYWORD = 0x1;
        private EventSource _dotNetRuntime;
        public static EventLevel _eventLevel = EventLevel.Informational;

        public static void EnableAllocationEvents()
        {
            _eventLevel = EventLevel.Verbose;
        }

        public Action<ulong, string> ProcessAllocEvent;
        public Action<ulong, ulong, ulong, ulong, ulong, ulong, ulong, ulong> ProcessHeapEvent;

        public GcEventListener(Action<ulong, string> processAllocEvent, Action<ulong, ulong, ulong, ulong, ulong, ulong, ulong, ulong> processHeapEvent)
        {
            ProcessAllocEvent = processAllocEvent;
            ProcessHeapEvent  = processHeapEvent;
        }

        protected override void OnEventSourceCreated(EventSource eventSource)
        {
            base.OnEventSourceCreated(eventSource);

            // GC Events are under Microsoft-Windows-DotNETRuntime
            if (eventSource.Name.Equals("Microsoft-Windows-DotNETRuntime"))
            {
                _dotNetRuntime = eventSource;
                EnableEvents(eventSource, _eventLevel, (EventKeywords)GC_KEYWORD);
            }
        }

        protected override void OnEventWritten(EventWrittenEventArgs eventData)
        {
            switch (eventData.EventName)
            {
                case "GCHeapStats_V1":
                    ProcessHeapStats(eventData);
                    break;
                case "GCAllocationTick_V3":
                    ProcessAllocationEvent(eventData);
                    break;
            }
        }

        private void ProcessAllocationEvent(EventWrittenEventArgs eventData)
        {
            ulong size = (ulong)eventData.Payload[3];
            string type = (string)eventData.Payload[5];
            ProcessAllocEvent(size, type);
        }

        private void ProcessHeapStats(EventWrittenEventArgs eventData)
        {
            ulong gen0Size = (ulong)eventData.Payload[0];
            ulong gen0Promoted = (ulong)eventData.Payload[1];
            ulong gen1Size = (ulong)eventData.Payload[2];
            ulong gen1Promoted = (ulong)eventData.Payload[3];
            ulong gen2Size = (ulong)eventData.Payload[4];
            ulong gen2Survived = (ulong)eventData.Payload[5];
            ulong lohSize = (ulong)eventData.Payload[6];
            ulong lohSurvived = (ulong)eventData.Payload[7];

            ProcessHeapEvent(gen0Size, gen0Promoted, gen1Size, gen1Promoted, gen2Size, gen2Survived, lohSize, lohSurvived);
        }
    }
}