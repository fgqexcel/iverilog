/*
 * Copyright (c) 2000Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: net_design.cc,v 1.27 2002/08/16 05:18:27 steve Exp $"
#endif

# include "config.h"

# include  <iostream>

/*
 * This source file contains all the implementations of the Design
 * class declared in netlist.h.
 */

# include  "netlist.h"
# include  "util.h"
# include  <strstream>

Design:: Design()
: errors(0), nodes_(0), procs_(0), lcounter_(0)
{
      procs_idx_ = 0;
      des_precision_ = 0;
      nodes_functor_cur_ = 0;
      nodes_functor_nxt_ = 0;
}

Design::~Design()
{
}

string Design::local_symbol(const string&path)
{
      strstream res;
      res << "_L" << (lcounter_++) << ends;

      return path + "." + res.str();
}

void Design::set_precision(int val)
{
      if (val < des_precision_)
	    des_precision_ = val;
}

int Design::get_precision() const
{
      return des_precision_;
}

unsigned long Design::scale_to_precision(unsigned long val,
					 const NetScope*scope) const
{
      int units = scope->time_unit();
      assert( units >= des_precision_ );

      while (units > des_precision_) {
	    units -= 1;
	    val *= 10;
      }

      return val;
}

NetScope* Design::make_root_scope(const char*root)
{
      NetScope *root_scope_;
      root_scope_ = new NetScope(0, root, NetScope::MODULE);
      root_scope_->set_module_name(root);
      root_scopes_.push_back(root_scope_);
      return root_scope_;
}

NetScope* Design::find_root_scope()
{
      assert(root_scopes_.front());
      return root_scopes_.front();
}

list<NetScope*> Design::find_root_scopes()
{
      return root_scopes_;
}

const list<NetScope*> Design::find_root_scopes() const
{
      return root_scopes_;
}

/*
 * This method locates a scope in the design, given its rooted
 * heirarchical name. Each component of the key is used to scan one
 * more step down the tree until the name runs out or the search
 * fails.
 */
NetScope* Design::find_scope(const hname_t&path) const
{
      if (path.peek_name(0) == 0)
	    return 0;

      for (list<NetScope*>::const_iterator scope = root_scopes_.begin()
		 ; scope != root_scopes_.end(); scope++) {

	    NetScope*cur = *scope;
	    if (strcmp(path.peek_name(0), cur->basename()) != 0)
		  continue;

	    unsigned hidx = 1;
	    while (cur) {
		  const char*name = path.peek_name(hidx);
		  if (name == 0)
			return cur;

		  cur = cur->child(name);
		  hidx += 1;
	    }

      }

      return 0;
}

/*
 * This is a relative lookup of a scope by name. The starting point is
 * the scope parameter is the place within which I start looking for
 * the scope. If I do not find the scope within the passed scope,
 * start looking in parent scopes until I find it, or I run out of
 * parent scopes.
 */
NetScope* Design::find_scope(NetScope*scope, const hname_t&path) const
{
      assert(scope);
      if (path.peek_name(0) == 0)
	    return scope;

      for ( ; scope ;  scope = scope->parent()) {
	    unsigned hidx = 0;
	    const char*key = path.peek_name(hidx);

	    NetScope*cur = scope;
	    do {
		  cur = cur->child(key);
		  if (cur == 0) break;
		  hidx += 1;
		  key = path.peek_name(hidx);
	    } while (key);

	    if (cur) return cur;
      }

	// Last chance. Look for the name starting at the root.
      return find_scope(path);
}

/*
 * Find a parameter from within a specified context. If the name is
 * not here, keep looking up until I run out of up to look at. The
 * method works by scanning scopes, starting with the passed scope and
 * working up towards the root, looking for the named parameter. The
 * name in this case can be hierarchical, so there is an inner loop to
 * follow the scopes of the name down to to key.
 */
const NetExpr* Design::find_parameter(const NetScope*scope,
				      const hname_t&path) const
{
      for ( ; scope ;  scope = scope->parent()) {
	    unsigned hidx = 0;

	    const NetScope*cur = scope;
	    while (path.peek_name(hidx+1)) {
		  cur = cur->child(path.peek_name(hidx));
		  if (cur == 0)
			break;
		  hidx += 1;
	    }

	    if (cur == 0)
		  continue;

	    if (const NetExpr*res = cur->get_parameter(path.peek_name(hidx)))
		  return res;
      }

      return 0;
}


/*
 * This method runs through the scope, noticing the defparam
 * statements that were collected during the elaborate_scope pass and
 * applying them to the target parameters. The implementation actually
 * works by using a specialized method from the NetScope class that
 * does all the work for me.
 */
void Design::run_defparams()
{
      for (list<NetScope*>::const_iterator scope = root_scopes_.begin(); 
	   scope != root_scopes_.end(); scope++)
	    (*scope)->run_defparams(this);
}

void NetScope::run_defparams(Design*des)
{
      { NetScope*cur = sub_;
        while (cur) {
	      cur->run_defparams(des);
	      cur = cur->sib_;
	}
      }

      map<hname_t,NetExpr*>::const_iterator pp;
      for (pp = defparams.begin() ;  pp != defparams.end() ;  pp ++ ) {
	    NetExpr*val = (*pp).second;
	    hname_t path = (*pp).first;

	    char*name = path.remove_tail_name();

	      /* If there is no path on the name, then the targ_scope
		 is the current scope. */
	    NetScope*targ_scope = des->find_scope(this, path);
	    if (targ_scope == 0) {
		  cerr << val->get_line() << ": warning: scope of " <<
			path << "." << name << " not found." << endl;
		  delete[]name;
		  continue;
	    }

	    NetExpr*tmp = targ_scope->set_parameter(name, val);
	    if (tmp == 0) {
		  cerr << val->get_line() << ": warning: parameter "
		       << name << " not found in " << targ_scope->name()
		       << "." << endl;
	    } else {
		  delete tmp;
	    }

	    delete[]name;
      }
}

void Design::evaluate_parameters()
{
      for (list<NetScope*>::const_iterator scope = root_scopes_.begin(); 
	   scope != root_scopes_.end(); scope++)
	    (*scope)->evaluate_parameters(this);
}

void NetScope::evaluate_parameters(Design*des)
{
      NetScope*cur = sub_;
      while (cur) {
	    cur->evaluate_parameters(des);
	    cur = cur->sib_;
      }


	// Evaluate the parameter values. The parameter expressions
	// have already been elaborated and replaced by the scope
	// scanning code. Now the parameter expression can be fully
	// evaluated, or it cannot be evaluated at all.

      typedef map<string,NetExpr*>::iterator mparm_it_t;

      for (mparm_it_t cur = parameters_.begin()
		 ; cur != parameters_.end() ;  cur ++) {

	      // Get the NetExpr for the parameter.
	    NetExpr*expr = (*cur).second;
	    assert(expr);

	      // If it's already a NetEConst, then this parameter is done.
	    if (dynamic_cast<const NetEConst*>(expr))
		  continue;

	      // Try to evaluate the expression.
	    NetExpr*nexpr = expr->eval_tree();
	    if (nexpr == 0) {
		  cerr << (*cur).second->get_line() << ": internal error: "
			"unable to evaluate parm expression: " <<
			*expr << endl;
		  des->errors += 1;
		  continue;
	    }

	      // The evaluate worked, replace the old expression with
	      // this constant value.
	    assert(nexpr);
	    delete expr;
	    (*cur).second = nexpr;
      }

}

string Design::get_flag(const string&key) const
{
      map<string,string>::const_iterator tmp = flags_.find(key);
      if (tmp == flags_.end())
	    return "";
      else
	    return (*tmp).second;
}

/*
 * This method looks for a signal (reg, wire, whatever) starting at
 * the specified scope. If the name is hierarchical, it is split into
 * scope and name and the scope used to find the proper starting point
 * for the real search.
 *
 * It is the job of this function to properly implement Verilog scope
 * rules as signals are concerned.
 */
NetNet* Design::find_signal(NetScope*scope, hname_t path)
{
      assert(scope);

      char*key = path.remove_tail_name();
      if (path.peek_name(0))
	    scope = find_scope(scope, path);

      while (scope) {
	    if (NetNet*net = scope->find_signal(key)) {
		  delete key;
		  return net;
	    }

	    if (scope->type() == NetScope::MODULE)
		  break;
	    scope = scope->parent();
      }

      delete key;
      return 0;
}

NetMemory* Design::find_memory(NetScope*scope, hname_t path)
{
      assert(scope);

      char*key = path.remove_tail_name();
      if (path.peek_name(0))
	    scope = find_scope(scope, path);

      while (scope) {
	    if (NetMemory*mem = scope->find_memory(key)) {
		  delete key;
		  return mem;
	    }

	    scope = scope->parent();
      }

      delete key;
      return 0;
}


NetFuncDef* Design::find_function(NetScope*scope, const hname_t&name)
{
      assert(scope);
      NetScope*func = find_scope(scope, name);
      if (func && (func->type() == NetScope::FUNC))
	    return func->func_def();

      return 0;
}

NetFuncDef* Design::find_function(const hname_t&key)
{
      NetScope*func = find_scope(key);
      if (func && (func->type() == NetScope::FUNC))
	    return func->func_def();

      return 0;
}

NetScope* Design::find_task(NetScope*scope, const hname_t&name)
{
      NetScope*task = find_scope(scope, name);
      if (task && (task->type() == NetScope::TASK))
	    return task;

      return 0;
}

NetScope* Design::find_task(const hname_t&key)
{
      NetScope*task = find_scope(key);
      if (task && (task->type() == NetScope::TASK))
	    return task;

      return 0;
}

NetEvent* Design::find_event(NetScope*scope, hname_t path)
{
      assert(scope);

      while (scope) {
	    if (NetEvent*ev = scope->find_event(path)) {
		  return ev;
	    }

	    if (scope->type() == NetScope::MODULE)
		  break;
	    scope = scope->parent();
      }

      return 0;
}

void Design::add_node(NetNode*net)
{
      assert(net->design_ == 0);
      if (nodes_ == 0) {
	    net->node_next_ = net;
	    net->node_prev_ = net;
      } else {
	    net->node_next_ = nodes_->node_next_;
	    net->node_prev_ = nodes_;
	    net->node_next_->node_prev_ = net;
	    net->node_prev_->node_next_ = net;
      }
      nodes_ = net;
      net->design_ = this;
}

void Design::del_node(NetNode*net)
{
      assert(net->design_ == this);
      assert(net != 0);

	/* Interact with the Design::functor method by manipulate the
	   cur and nxt pointers that is is using. */
      if (net == nodes_functor_nxt_)
	    nodes_functor_nxt_ = nodes_functor_nxt_->node_next_;
      if (net == nodes_functor_nxt_)
	    nodes_functor_nxt_ = 0;

      if (net == nodes_functor_cur_)
	    nodes_functor_cur_ = 0;

	/* Now perform the actual delete. */
      if (nodes_ == net)
	    nodes_ = net->node_prev_;

      if (nodes_ == net) {
	    nodes_ = 0;
      } else {
	    net->node_next_->node_prev_ = net->node_prev_;
	    net->node_prev_->node_next_ = net->node_next_;
      }

      net->design_ = 0;
}

void Design::add_process(NetProcTop*pro)
{
      pro->next_ = procs_;
      procs_ = pro;
}

void Design::delete_process(NetProcTop*top)
{
      assert(top);
      if (procs_ == top) {
	    procs_ = top->next_;

      } else {
	    NetProcTop*cur = procs_;
	    while (cur->next_ != top) {
		  assert(cur->next_);
		  cur = cur->next_;
	    }

	    cur->next_ = top->next_;
      }

      if (procs_idx_ == top)
	    procs_idx_ = top->next_;

      delete top;
}

/*
 * $Log: net_design.cc,v $
 * Revision 1.27  2002/08/16 05:18:27  steve
 *  Fix intermix of node functors and node delete.
 *
 * Revision 1.26  2002/08/12 01:34:59  steve
 *  conditional ident string using autoconfig.
 *
 * Revision 1.25  2002/07/03 05:34:59  steve
 *  Fix scope search for events.
 *
 * Revision 1.24  2002/06/25 02:39:34  steve
 *  Fix mishandling of incorect defparam error message.
 *
 * Revision 1.23  2001/12/03 04:47:15  steve
 *  Parser and pform use hierarchical names as hname_t
 *  objects instead of encoded strings.
 *
 * Revision 1.22  2001/10/20 05:21:51  steve
 *  Scope/module names are char* instead of string.
 *
 * Revision 1.21  2001/10/19 21:53:24  steve
 *  Support multiple root modules (Philip Blundell)
 *
 * Revision 1.20  2001/07/25 03:10:49  steve
 *  Create a config.h.in file to hold all the config
 *  junk, and support gcc 3.0. (Stephan Boettcher)
 *
 * Revision 1.19  2001/04/02 02:28:12  steve
 *  Generate code for task calls.
 *
 * Revision 1.18  2001/01/14 23:04:56  steve
 *  Generalize the evaluation of floating point delays, and
 *  get it working with delay assignment statements.
 *
 *  Allow parameters to be referenced by hierarchical name.
 *
 * Revision 1.17  2000/12/16 01:45:48  steve
 *  Detect recursive instantiations (PR#2)
 *
 * Revision 1.16  2000/09/24 17:41:13  steve
 *  fix null pointer when elaborating undefined task.
 *
 * Revision 1.15  2000/08/26 00:54:03  steve
 *  Get at gate information for ivl_target interface.
 *
 * Revision 1.14  2000/08/12 17:59:48  steve
 *  Limit signal scope search at module boundaries.
 *
 * Revision 1.13  2000/07/30 18:25:43  steve
 *  Rearrange task and function elaboration so that the
 *  NetTaskDef and NetFuncDef functions are created during
 *  signal enaboration, and carry these objects in the
 *  NetScope class instead of the extra, useless map in
 *  the Design class.
 *
 * Revision 1.12  2000/07/23 02:41:32  steve
 *  Excessive assert.
 *
 * Revision 1.11  2000/07/22 22:09:03  steve
 *  Parse and elaborate timescale to scopes.
 *
 * Revision 1.10  2000/07/16 04:56:08  steve
 *  Handle some edge cases during node scans.
 *
 * Revision 1.9  2000/07/14 06:12:57  steve
 *  Move inital value handling from NetNet to Nexus
 *  objects. This allows better propogation of inital
 *  values.
 *
 *  Clean up constant propagation  a bit to account
 *  for regs that are not really values.
 *
 * Revision 1.8  2000/05/02 16:27:38  steve
 *  Move signal elaboration to a seperate pass.
 *
 * Revision 1.7  2000/05/02 03:13:31  steve
 *  Move memories to the NetScope object.
 *
 * Revision 1.6  2000/05/02 00:58:12  steve
 *  Move signal tables to the NetScope class.
 *
 * Revision 1.5  2000/04/28 16:50:53  steve
 *  Catch memory word parameters to tasks.
 *
 * Revision 1.4  2000/04/10 05:26:06  steve
 *  All events now use the NetEvent class.
 *
 * Revision 1.3  2000/03/11 03:25:52  steve
 *  Locate scopes in statements.
 *
 * Revision 1.2  2000/03/10 06:20:48  steve
 *  Handle defparam to partial hierarchical names.
 *
 * Revision 1.1  2000/03/08 04:36:53  steve
 *  Redesign the implementation of scopes and parameters.
 *  I now generate the scopes and notice the parameters
 *  in a separate pass over the pform. Once the scopes
 *  are generated, I can process overrides and evalutate
 *  paremeters before elaboration begins.
 *
 */

