using MyProduct.Entities.Model;
using MyProduct.Entities.Repositories;
using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Business
{
    [ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
    public class CustomerService : ICustomerService
    {
        private readonly ICustomerRepository _customerRepository;
        private int _idCounter = 0;
        private readonly ILogger _logger;
        public CustomerService(ICustomerRepository customerRepository, ILogger logger)
        {
            _customerRepository = customerRepository;
            _logger = logger.ForContext<CustomerService>();
        }

        public void AddCustomer(string name, string address)
        {
            _customerRepository.AddCustomer(new Customer { Id = ++_idCounter, Name = name, Address = address });
        }

        public IEnumerable<Customer> GetCustomers()
        {
            try
            {
                throw new Exception("Can sıkıntısı.");
            }
            catch (Exception e)
            {
                _logger.Error(e, "Bir hata oluştu.{@Customer}", new Customer { Id = 123, Name = "sadfa", Address = " asdasd"});
            }
            return _customerRepository.GetCustomers();
        }

        public void SubmitComplain(Customer customer, string text)
        {
            using (var sw = new StreamWriter(@"C:\temp\myproduct.txt", true))
            {
                sw.WriteLine($"{customer.Id} {text}");
            }
        }
    }
}
