using MyProduct.Entities.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Business
{
    [ServiceContract]
    public interface ICustomerService
    {
        [OperationContract]
        void AddCustomer(string name, string address);
        [OperationContract]
        IEnumerable<Customer> GetCustomers();
        [OperationContract]
        void SubmitComplain(Customer customer, string text);
    }
}
