using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WaitEventPoC
{
    class BackupManager
    {
        private ManualResetEvent _resetEvent = new ManualResetEvent(true);
        private CancellationTokenSource _cts = new CancellationTokenSource();
        private bool _isStarted = false;
        public void CreateBackup()
        {
            _isStarted = true;
            var ct = _cts.Token;
            Task.Run(() => {
                for (int i = 0; i < 100; i++)
                {
                    if (ct.IsCancellationRequested)
                    {
                        //cleanup 
                        Console.WriteLine($"Job cancelled at %{i}");
                        return;
                    }
                    _resetEvent.WaitOne();
                    Thread.Sleep(500);
                    Console.WriteLine($"%{i + 1} Completed");
                    //do some backup thing
                }

            });
        }

        public void Pause()
        {
            if (!_isStarted) throw new Exception("Backup is not started");
            _resetEvent.Reset();
        }

        public void Resume()
        {
            if (!_isStarted) throw new Exception("Backup is not started");
            _resetEvent.Set();
        }


        public void CancelTask()
        {
            _cts.Cancel();
            _isStarted = false;
            _resetEvent.Set();
        }


    }
}
