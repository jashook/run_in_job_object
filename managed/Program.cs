using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;

using ev30;

namespace managed
{
    class Program
    {
        private static void Allocate()
        {
            while (true)
            {
                byte[] arrayCreated = new byte[50 * 1024 * 1024];
                for (int index = 0; index < arrayCreated.Length; ++index)
                {
                    char data = (char)(index % char.MaxValue);

                    arrayCreated[index] = (byte)data;
                }
            }
        }

        static void Main(string[] args)
        {
            GcEventListener.EnableAllocationEvents();
            new GcEventListener((ulong size, string type) => {
                Console.WriteLine($"[{type}]: {size} -- Allocated");
            }, (ulong gen0Size, ulong gen0Promoted, ulong gen1Size, ulong gen1Promoted, ulong gen2Size, ulong gen2Survived, ulong lohSize, ulong lohSurvived) => {
                Console.WriteLine($"[GC Event]: Gen0Size: {gen0Size}, Gen0Promoted: {gen0Promoted}, Gen1Size: {gen1Size}, Gen1Promoted: {gen1Promoted}, Gen2Size: {gen2Size}, Gen2Survived: {gen2Survived}, LOHSize: {lohSize}, LOHSurvived: {lohSurvived}");
            });

            int threadCount = 50;
            for (int i = 0; i < threadCount; ++i) Task.Run(() => { Allocate(); });

            Allocate();
        }
    }
}
