using MyProduct.Entities.Model;
using MyProduct.Entities.Repositories;
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
        public CustomerService(ICustomerRepository customerRepository)
        {
            _customerRepository = customerRepository;
        }

        public void AddCustomer(string name, string address)
        {
            _customerRepository.AddCustomer(new Customer { Id = ++_idCounter, Name = name, Address = address });
        }

        public IEnumerable<Customer> GetCustomers()
        {
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
