using Quartz;
using Quartz.Impl;
using Quartz.Impl.Calendar;
using QuartzPooc;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPoC
{
    class Program
    {
        private static IScheduler _scheduler;
        static async Task Main(string[] args)
        {
            int id = 5;
            NameValueCollection props = new NameValueCollection
                {
                    { "quartz.serializer.type", "binary" }
                };
            StdSchedulerFactory factory = new StdSchedulerFactory(props);
            _scheduler = await factory.GetScheduler();
            var jobFactory = new MyJobFactory(new TaskRepository());
            var job = JobBuilder.Create<MyBackupJob>()
                .WithIdentity($"myjob{id}", "narbulut_jobs")
                .UsingJobData("parameterId", id)
                .Build();
            var trigger = TriggerBuilder.Create()
                .WithIdentity($"trigger{id}", "nar_jobs")
                .WithSimpleSchedule(x => x
                    .WithIntervalInSeconds(10)
                    .WithRepeatCount(10))
                .Build();
            await _scheduler.ScheduleJob(job, trigger);
            _scheduler.JobFactory = jobFactory;
            await _scheduler.Start();
            await Console.In.ReadLineAsync();
        }

    }
}
