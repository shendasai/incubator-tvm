/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file tvm/runtime/module.h
 * \brief Runtime container of the functions generated by TVM,
 *  This is used to support dynamically link, load and save
 *  functions from different convention under unified API.
 */
#ifndef TVM_RUNTIME_MODULE_H_
#define TVM_RUNTIME_MODULE_H_

#include <dmlc/io.h>
#include <tvm/runtime/c_runtime_api.h>
#include <tvm/runtime/memory.h>
#include <tvm/runtime/object.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tvm {
namespace runtime {

class ModuleNode;
class PackedFunc;

/*!
 * \brief Module container of TVM.
 */
class Module : public ObjectRef {
 public:
  Module() {}
  // constructor from container.
  explicit Module(ObjectPtr<Object> n) : ObjectRef(n) {}
  /*!
   * \brief Get packed function from current module by name.
   *
   * \param name The name of the function.
   * \param query_imports Whether also query dependency modules.
   * \return The result function.
   *  This function will return PackedFunc(nullptr) if function do not exist.
   * \note Implemented in packed_func.cc
   */
  inline PackedFunc GetFunction(const std::string& name, bool query_imports = false);
  // The following functions requires link with runtime.
  /*!
   * \brief Import another module into this module.
   * \param other The module to be imported.
   *
   * \note Cyclic dependency is not allowed among modules,
   *  An error will be thrown when cyclic dependency is detected.
   */
  inline void Import(Module other);
  /*! \return internal container */
  inline ModuleNode* operator->();
  /*! \return internal container */
  inline const ModuleNode* operator->() const;
  /*!
   * \brief Load a module from file.
   * \param file_name The name of the host function module.
   * \param format The format of the file.
   * \note This function won't load the import relationship.
   *  Re-create import relationship by calling Import.
   */
  TVM_DLL static Module LoadFromFile(const std::string& file_name, const std::string& format = "");
  // refer to the corresponding container.
  using ContainerType = ModuleNode;
  friend class ModuleNode;
};

/*!
 * \brief Base container of module.
 *
 * Please subclass ModuleNode to create a specific runtime module.
 *
 * \code
 *
 *  class MyModuleNode : public ModuleNode {
 *   public:
 *    // implement the interface
 *  };
 *
 *  // use make_object to create a specific
 *  // instace of MyModuleNode.
 *  Module CreateMyModule() {
 *    ObjectPtr<MyModuleNode> n =
 *      tvm::runtime::make_object<MyModuleNode>();
 *    return Module(n);
 *  }
 *
 * \endcode
 */
class TVM_DLL ModuleNode : public Object {
 public:
  /*! \brief virtual destructor */
  virtual ~ModuleNode() {}
  /*!
   * \return The per module type key.
   * \note This key is used to for serializing custom modules.
   */
  virtual const char* type_key() const = 0;
  /*!
   * \brief Get a PackedFunc from module.
   *
   *  The PackedFunc may not be fully initialized,
   *  there might still be first time running overhead when
   *  executing the function on certain devices.
   *  For benchmarking, use prepare to eliminate
   *
   * \param name the name of the function.
   * \param sptr_to_self The ObjectPtr that points to this module node.
   *
   * \return PackedFunc(nullptr) when it is not available.
   *
   * \note The function will always remain valid.
   *   If the function need resource from the module(e.g. late linking),
   *   it should capture sptr_to_self.
   */
  virtual PackedFunc GetFunction(const std::string& name,
                                 const ObjectPtr<Object>& sptr_to_self) = 0;
  /*!
   * \brief Save the module to file.
   * \param file_name The file to be saved to.
   * \param format The format of the file.
   */
  virtual void SaveToFile(const std::string& file_name, const std::string& format);
  /*!
   * \brief Save the module to binary stream.
   * \param stream The binary stream to save to.
   * \note It is recommended to implement this for device modules,
   *   but not necessarily host modules.
   *   We can use this to do AOT loading of bundled device functions.
   */
  virtual void SaveToBinary(dmlc::Stream* stream);
  /*!
   * \brief Get the source code of module, when available.
   * \param format Format of the source code, can be empty by default.
   * \return Possible source code when available.
   */
  virtual std::string GetSource(const std::string& format = "");
  /*!
   * \brief Get packed function from current module by name.
   *
   * \param name The name of the function.
   * \param query_imports Whether also query dependency modules.
   * \return The result function.
   *  This function will return PackedFunc(nullptr) if function do not exist.
   * \note Implemented in packed_func.cc
   */
  PackedFunc GetFunction(const std::string& name, bool query_imports = false);
  /*!
   * \brief Import another module into this module.
   * \param other The module to be imported.
   *
   * \note Cyclic dependency is not allowed among modules,
   *  An error will be thrown when cyclic dependency is detected.
   */
  void Import(Module other);
  /*!
   * \brief Get a function from current environment
   *  The environment includes all the imports as well as Global functions.
   *
   * \param name name of the function.
   * \return The corresponding function.
   */
  const PackedFunc* GetFuncFromEnv(const std::string& name);
  /*! \return The module it imports from */
  const std::vector<Module>& imports() const { return imports_; }

  // integration with the existing components.
  static constexpr const uint32_t _type_index = TypeIndex::kRuntimeModule;
  static constexpr const char* _type_key = "runtime.Module";
  // NOTE: ModuleNode can still be sub-classed
  //
  TVM_DECLARE_FINAL_OBJECT_INFO(ModuleNode, Object);

 protected:
  friend class Module;
  friend class ModuleInternal;
  /*! \brief The modules this module depend on */
  std::vector<Module> imports_;

 private:
  /*! \brief Cache used by GetImport */
  std::unordered_map<std::string, std::shared_ptr<PackedFunc> > import_cache_;
};

/*!
 * \brief Check if runtime module is enabled for target.
 * \param target The target module name.
 * \return Whether runtime is enabled.
 */
TVM_DLL bool RuntimeEnabled(const std::string& target);

/*! \brief namespace for constant symbols */
namespace symbol {
/*! \brief Global variable to store module context. */
constexpr const char* tvm_module_ctx = "__tvm_module_ctx";
/*! \brief Global variable to store device module blob */
constexpr const char* tvm_dev_mblob = "__tvm_dev_mblob";
/*! \brief Number of bytes of device module blob. */
constexpr const char* tvm_dev_mblob_nbytes = "__tvm_dev_mblob_nbytes";
/*! \brief global function to set device */
constexpr const char* tvm_set_device = "__tvm_set_device";
/*! \brief Auxiliary counter to global barrier. */
constexpr const char* tvm_global_barrier_state = "__tvm_global_barrier_state";
/*! \brief Prepare the global barrier before kernels that uses global barrier. */
constexpr const char* tvm_prepare_global_barrier = "__tvm_prepare_global_barrier";
/*! \brief Placeholder for the module's entry function. */
constexpr const char* tvm_module_main = "__tvm_main__";
}  // namespace symbol

// implementations of inline functions.

inline void Module::Import(Module other) { return (*this)->Import(other); }

inline ModuleNode* Module::operator->() { return static_cast<ModuleNode*>(get_mutable()); }

inline const ModuleNode* Module::operator->() const {
  return static_cast<const ModuleNode*>(get());
}

}  // namespace runtime
}  // namespace tvm

#include <tvm/runtime/packed_func.h>  // NOLINT(*)
#endif                                // TVM_RUNTIME_MODULE_H_
