using MyProduct.Entities.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Entities.Repositories
{
    public interface ICustomerRepository
    {
        IEnumerable<Customer> GetCustomers();
        Customer AddCustomer(Customer customer);
        void DeleteCustomer(Customer customer);
        Customer UpdateCustomer(Customer customer);
        Customer GetCustomerById(int id);
    }
}
