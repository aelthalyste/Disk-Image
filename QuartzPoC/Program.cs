using Quartz;
using Quartz.Impl;
using Quartz.Impl.Calendar;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPoC
{
    class Program
    {
        static async Task Main(string[] args)
        {
            await RRRRRRRRR();
            Console.ReadLine();
            HolidayCalendar cal = new HolidayCalendar();
        }

        private static async Task RRRRRRRRR()
        {
            // construct a scheduler factory
            StdSchedulerFactory factory = new StdSchedulerFactory();

            // get a scheduler
            IScheduler scheduler = await factory.GetScheduler();
            await scheduler.Start();

            // define the job and tie it to our HelloJob class
            IJobDetail job = JobBuilder.Create<HelloJob>()
                .WithIdentity("myJob", "group1")
                .Build();

            // Trigger the job to run now, and then every 40 seconds
            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("myTrigger", "group1")
                .StartNow()
                .WithSimpleSchedule(x => x
                    .WithIntervalInSeconds(3)
                    .RepeatForever())
            .Build();

            await scheduler.ScheduleJob(job, trigger);
        }

        public class HelloJob : IJob
        {
            public async Task Execute(IJobExecutionContext context)
            {
                await Console.Out.WriteLineAsync("Greetings from HelloJob!");
            }
        }
    }
}
