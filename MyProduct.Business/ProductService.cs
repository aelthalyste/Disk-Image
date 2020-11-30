using MyProduct.Entities.Model;
using Serilog;
using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Business
{
    [ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
    public class ProductService : IProductService
    {
        private readonly ILogger _logger;

        public ProductService(ILogger logger)
        {
            _logger = logger.ForContext<ProductService>();
        }

        public IEnumerable<Product> GetProductList()
        {
            _logger.Debug("GetProduct list is called");
            return new List<Product> { 
                new Product { Id = 1, Description = "some product", Name = "Product1" }, 
                new Product { Id = 2, Description = "some product", Name = "Product2" } };
        }

        public Product RegisterProduct(string name, string description)
        {
            return new Product { Id = 3, Name = name, Description = description };
        }
    }
}
