/*
 *  Copyright (C) 2011 John Veitch
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

/**
 * \file
 * \ingroup XML
 * \brief Implementation of the VOTable serializers XML LALInference API
 */

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <lal/XLALError.h>

#include <lal/LALXML.h>
#include <lal/LALXMLVOTableCommon.h>
#include <lal/LALXMLVOTableSerializers.h>
#include <lal/LALInferenceXML.h>

#define INT4STR_MAXLEN          15
#define REAL8STR_MAXLEN         25
#define NAMESTR_MAXLEN          256

/**
 * \brief Serializes an array of \c LALInferenceVariables into a VOTable XML %node
 *
 * This function takes a \c LALInferenceVariables structure and serializes it into a VOTable
 * \c RESOURCE %node identified by the given name. The returned \c xmlNode can then be
 * embedded into an existing %node hierarchy or turned into a full VOTable document.
 * A VOTable Table element is returned, with fixed variables as PARAMs and the varying ones as FIELDs.
 * 
 * \param varsArray [in] Pointer to an array of \c LALInferenceVariables structures to be serialized
 * \param N [in] Number of items in the array
 * 
 * \return A pointer to a \c xmlNode that holds the VOTable fragment that represents
 * the \c LALInferenceVariables array.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * fragment isn't needed anymore) using \c xmlFreeNode. Alternatively, \c xmlFreeDoc
 * can be used later on when the returned fragment has been embedded in a XML document.
 *
 * \sa XLALCreateVOTParamNode
 * \sa XLALCreateVOTResourceNode
 *
 * \author John Veitch\n
 * 
 */



xmlNodePtr XLALInferenceVariablesArray2VOTTable(const LALInferenceVariables *varsArray, UINT4 N)
{
  xmlNodePtr fieldNodeList=NULL;
  xmlNodePtr paramNodeList=NULL;
  xmlNodePtr xmlTABLEDATAnode=NULL;
  xmlNodePtr VOTtableNode=NULL;
  xmlNodePtr tmpNode=NULL;
  xmlNodePtr field_ptr,param_ptr;
  LALInferenceVariableItem *varitem=NULL;
  void **valuearrays=NULL;
  UINT4 Nfields=0,i,j;
  const char *fn = __func__;
  char *tablename=NULL;
  int err;

  
	/* Sanity check input */
	if(!varsArray) {
		XLALPrintError("Received null varsArray pointer");
		XLAL_ERROR_NULL(__func__,XLAL_EFAULT);
	}
	if(N==0) return(NULL);
	
	field_ptr=fieldNodeList;
	param_ptr=paramNodeList;
	varitem=varsArray[0].head;
	
  /* Build a list of PARAM and FIELD elements */
  while(varitem)
	{
		switch(varitem->vary){
			case LALINFERENCE_PARAM_LINEAR:
			case LALINFERENCE_PARAM_CIRCULAR:
			case LALINFERENCE_PARAM_OUTPUT:
			{
				tmpNode=LALInferenceVariableItem2VOTFieldNode(varitem);
				field_ptr=xmlAddNextSibling(field_ptr,tmpNode);
				Nfields++;
				break;
			}
			case LALINFERENCE_PARAM_FIXED:
			{
				tmpNode=LALInferenceVariableItem2VOTParamNode(varitem);
				param_ptr=xmlAddNextSibling(param_ptr,tmpNode);
				break;
			}
			default: 
			{
				XLALPrintError("Unknown param vary type");
			}
		}
		varitem=varitem->next;
	}

  valuearrays=calloc(Nfields,sizeof(void *));
  VOTABLE_DATATYPE *dataTypes=calloc(Nfields,sizeof(VOTABLE_DATATYPE));
  /* Build array of DATA */
  for(field_ptr=fieldNodeList,j=0;field_ptr!=NULL;field_ptr=field_ptr->next,j++)
  {
    const xmlChar *name=field_ptr->name;
    char parname[VARNAME_MAX];
    sprintf(parname,"%s",name);
    UINT4 typesize=LALInferenceTypeSize[LALInferenceGetVariableType(&varsArray[0],parname)];
    valuearrays[j]=calloc(N,typesize);
    dataTypes[j]=field_ptr->type;
    for(i=0;i<N;i++)
    {
      memcpy((char *)valuearrays[j]+i*typesize,LALInferenceGetVariable(&varsArray[i],parname),typesize);    
    }
  }
  UINT4 row,col;
  
     /* create TABLEDATA node */
    if ( ( xmlTABLEDATAnode = xmlNewNode ( NULL, CAST_CONST_XMLCHAR("TABLEDATA") ))== NULL ) {
      XLALPrintError ("%s: xmlNewNode() failed to create 'TABLEDATA' node.\n", fn );
      err = XLAL_ENOMEM;
      goto failed;
    }

    /* ---------- loop over data-arrays and generate each table-row */
    for ( row = 0; row < N; row ++ )
      {
        /* create TR node */
        xmlNodePtr xmlThisRowNode = NULL;
        if ( (xmlThisRowNode = xmlNewNode ( NULL, CAST_CONST_XMLCHAR("TR") )) == NULL ) {
          XLALPrintError ("%s: xmlNewNode() failed to create new 'TR' node.\n", fn );
          err = XLAL_EFAILED;
          goto failed;
        }
        if ( xmlAddChild(xmlTABLEDATAnode, xmlThisRowNode ) == NULL ) {
          XLALPrintError ("%s: failed to insert 'TR' node into 'TABLEDATA' node.\n", fn );
          err = XLAL_EFAILED;
          goto failed;
        }

        /* ----- loop over columns and generate each table element */
        for ( col = 0; col < Nfields; col ++ )
          {
            /* create TD node */
            xmlNodePtr xmlThisEntryNode = NULL;
            if ( (xmlThisEntryNode = xmlNewNode ( NULL, CAST_CONST_XMLCHAR("TD") )) == NULL ) {
              XLALPrintError ("%s: xmlNewNode() failed to create new 'TD' node.\n", fn );
              err = XLAL_EFAILED;
              goto failed;
            }
            if ( xmlAddChild(xmlThisRowNode, xmlThisEntryNode ) == NULL ) {
              XLALPrintError ("%s: failed to insert 'TD' node into 'TR' node.\n", fn );
              err = XLAL_EFAILED;
              goto failed;
            }

            const char* tmptxt;
            if ( (tmptxt = XLALVOTprintfFromArray ( dataTypes[col], NULL, valuearrays[col], row )) == NULL ){
              XLALPrintError ("%s: XLALVOTprintfFromArray() failed for row = %d, col = %d. errno = %d.\n", fn, row, col, xlalErrno );
              err = XLAL_EFUNC;
              goto failed;
            }

            xmlNodePtr xmlTextNode;
            if ( (xmlTextNode = xmlNewText (CAST_CONST_XMLCHAR(tmptxt) )) == NULL ) {
              XLALPrintError("%s: xmlNewText() failed to turn text '%s' into node\n", fn, tmptxt );
              err = XLAL_EFAILED;
              goto failed;
            }
            if ( xmlAddChild(xmlThisEntryNode, xmlTextNode ) == NULL ) {
              XLALPrintError ("%s: failed to insert text-node node into 'TD' node.\n", fn );
              err = XLAL_EFAILED;
              goto failed;
            }

          } /* for col < numFields */

      } /* for row < numRows */

  
  
  /* Create a TABLE from the FIELDs and TABLEDATA nodes */
  sprintf(tablename,"LALInferenceXMLTable");
  VOTtableNode= XLALCreateVOTTableNode (tablename, fieldNodeList, xmlTABLEDATAnode );
  
  /* Attach PARAMs to TABLE node */
  if(!xmlAddChildList(VOTtableNode,paramNodeList)){
    XLALPrintError("%s: xmlAddChildList failed\n",fn);
    err=XLAL_EFAILED;
    goto failed;
  }
  
  return(VOTtableNode);
  
  failed:
      XLAL_ERROR_NULL ( fn, err );

  return(NULL);
  
}

/**
 * \brief Serializes a \c LALInferenceVariables structure into a VOTable XML %node
 *
 * This function takes a \c LALInferenceVariables structure and serializes it into a VOTable
 * \c RESOURCE %node identified by the given name. The returned \c xmlNode can then be
 * embedded into an existing %node hierarchy or turned into a full VOTable document.
 *
 * \param vars [in] Pointer to the \c LALInferenceVariables structure to be serialized
 * \param name [in] Unique identifier of this particular \c LALInferenceVariables structure instance
 *
 * \return A pointer to a \c xmlNode that holds the VOTable fragment that represents
 * the \c LALInferenceVariables structure.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * fragment isn't needed anymore) using \c xmlFreeNode. Alternatively, \c xmlFreeDoc
 * can be used later on when the returned fragment has been embedded in a XML document.
 *
 * \sa LALInferenceVariableItem2VOTParamNode
 *
 * \author John Veitch\n
 * 
 */
xmlNodePtr XLALInferenceVariables2VOTNode (const LALInferenceVariables *const vars,const char *name)
{
  
  /* set up local variables */
  const char *fn = __func__;
  xmlNodePtr xmlChildNodeList = NULL;
  LALInferenceVariableItem *marker=vars->head;
  xmlNodePtr *xmlChildNodePtr=&xmlChildNodeList;

  
  /* Walk through the LALInferenceVariables adding each one */
  while(marker){
    *xmlChildNodePtr = (LALInferenceVariableItem2VOTParamNode(marker));
        if(!*xmlChildNodePtr) {
        /* clean up */
        xmlFreeNodeList(xmlChildNodeList);
        XLALPrintError("Couldn't create PARAM node: %s.%s\n", name,marker->name);
        XLAL_ERROR_NULL(fn, XLAL_EFAILED);
    }
    marker=marker->next;
    xmlChildNodePtr = &((*xmlChildNodePtr)->next);
  }
  return(xmlChildNodeList);
}

/**
 * \brief Serializes a \c LALInferenceVariableItem structure into a VOTable XML %node
 *
 * This function takes a \c LALInferenceVariableItem structure and serializes it into a VOTable
 * \c RESOURCE %node identified by the given name. The returned \c xmlNode can then be
 * embedded into an existing %node hierarchy or turned into a full VOTable document.
 *
 * \param varitem [in] Pointer to the \c LALInferenceVariables structure to be serialized
 * 
 * \return A pointer to a \c xmlNode that holds the VOTable fragment that represents
 * the \c LALInferenceVariableItem structure.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * fragment isn't needed anymore) using \c xmlFreeNode. Alternatively, \c xmlFreeDoc
 * can be used later on when the returned fragment has been embedded in a XML document.
 *
 * \sa XLALCreateVOTParamNode
 * 
 * \author John Veitch\n
 * 
 */

xmlNodePtr LALInferenceVariableItem2VOTFieldNode(LALInferenceVariableItem *varitem)
{
  VOTABLE_DATATYPE vo_type;
  CHAR *unitName={0};
  
  /* Special case for matrix */
  if(varitem->type==LALINFERENCE_gslMatrix_t)
    return(XLALgsl_matrix2VOTNode((gsl_matrix *)varitem->value, varitem->name, unitName));
	
	/* Special case for string */
	if(varitem->type==LALINFERENCE_string_t)
		return(XLALCreateVOTFieldNode(varitem->name,unitName,VOT_CHAR,"*"));
	
  /* Check the type of the item */
  vo_type=LALInferenceVariableType2VOT(varitem->type);
	
  return(XLALCreateVOTFieldNode(varitem->name,unitName,vo_type,NULL));
}

xmlNodePtr LALInferenceVariableItem2VOTParamNode(LALInferenceVariableItem *varitem)
{
  VOTABLE_DATATYPE vo_type;
  CHAR *unitName={0};
  CHAR valString[VARVALSTRINGSIZE_MAX]="";
  
  /* Special case for matrix */
  if(varitem->type==LALINFERENCE_gslMatrix_t)
    return(XLALgsl_matrix2VOTNode((gsl_matrix *)varitem->value, varitem->name, unitName));

	/* Special case for string */
	if(varitem->type==LALINFERENCE_string_t)
		return(XLALCreateVOTParamNode(varitem->name,unitName,VOT_CHAR,"*",varitem->value));
	
  /* Check the type of the item */
  vo_type=LALInferenceVariableType2VOT(varitem->type);
  
  LALInferencePrintVariableItem(valString, varitem);
  
  return(XLALCreateVOTParamNode(varitem->name,unitName,vo_type,NULL,valString));
}

/**
 * \brief Convert a \c LALInferenceVariableType into a VOType
 */
VOTABLE_DATATYPE LALInferenceVariableType2VOT(const LALInferenceVariableType litype){
  
  switch(litype){
    case LALINFERENCE_INT4_t: 		return VOT_INT4;
    case LALINFERENCE_INT8_t: 		return VOT_INT8;
    case LALINFERENCE_UINT4_t: 		return VOT_INT8; /* Need a signed INT8 to store an unsigned UINT4 */
    case LALINFERENCE_REAL4_t:		return VOT_REAL4;
    case LALINFERENCE_REAL8_t:		return VOT_REAL8;
    case LALINFERENCE_COMPLEX8_t: 	return VOT_COMPLEX8;
    case LALINFERENCE_COMPLEX16_t:	return VOT_COMPLEX16;
    case LALINFERENCE_string_t:		return VOT_CHAR;
    default: {XLALPrintError("Unsupported LALInferenceVarableType"); return VOT_DATATYPE_LAST;}
  }
}

  