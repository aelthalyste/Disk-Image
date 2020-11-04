using Quartz;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPooc
{
    class MyBackupJob : IJob
    {
        public MyBackupJob()
        {

        }
        public async Task Execute(IJobExecutionContext context)
        {
            int id = (int)context.JobDetail.JobDataMap["parameterId"];            
            Console.WriteLine($"Execute task {id}. Refire: {context.RefireCount}"); // task execution
            await Task.Delay(1000);
            JobExecutionException exception = null;
            bool result = true; 
            try
            {
                //do some business task
                result = false;
            }
            catch (Exception e)
            {
                exception = new JobExecutionException(e, context.RefireCount < 5);
            }
            if (!result) exception = new JobExecutionException(context.RefireCount < 5);
            if (exception != null)
            {
                await Task.Delay(TimeSpan.FromMinutes(3));
                throw exception;
            }
        }
    }
}
