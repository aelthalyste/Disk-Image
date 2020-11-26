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
    public interface IProductService
    {
        [OperationContract]
        Product RegisterProduct(string name, string description);
        [OperationContract]
        IEnumerable<Product> GetProductList();
    }
}
