using MyProduct.Entities.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Entities.Repositories
{
    public class CustomerRepository : ICustomerRepository
    {
        private static List<Customer> _customers = new List<Customer> { new Customer { Id = 1234, Name = "Ali Veli", Address="Çıkmaz sokak. Daire: 666"} };
        public Customer AddCustomer(Customer customer)
        {
            _customers.Add(customer);
            return customer;
        }

        public void DeleteCustomer(Customer customer)
        {
            _customers.Remove(_customers.FirstOrDefault(c => c.Id == customer.Id));
        }

        public Customer GetCustomerById(int id)
        {
            return _customers.FirstOrDefault(c => c.Id == id);
        }

        public IEnumerable<Customer> GetCustomers()
        {
            return _customers;
        }

        public Customer UpdateCustomer(Customer customer)
        {
            var c1 = _customers.FirstOrDefault(c => c.Id == customer.Id);
            c1.Name = customer.Name;
            c1.Address = customer.Address;
            return c1;
        }
    }
}
